#include "vbomesh.h"

#include <cstdlib>
#include <iostream>
using std::cout;
using std::cerr;
using std::endl;
#include <fstream>
using std::ifstream;
#include <sstream>
using std::istringstream;

VBOMesh::VBOMesh(const char * fileName, bool center, bool loadTc, bool genTangents) :
        nFaces(0), v(0), nVerts(0), n(0), tc(0), el(0), tang(0),
        reCenterMesh(center), loadTex(loadTc), genTang(genTangents)
{
    loadOBJ(fileName);
}

VBOMesh::~VBOMesh()
{
    delete [] v;
    delete [] n;
    if( tc != NULL ) delete [] tc;
    if( tang != NULL ) delete [] tang;
    delete [] el;
}

void VBOMesh::loadOBJ( const char * fileName ) {

    vector <QVector3D> points;
    vector <QVector3D> normals;
    vector <QVector2D> texCoords;
    vector <unsigned int> faces;

    ifstream objStream( fileName, std::ios::in );

    if( !objStream ) {
        cerr << "Unable to open OBJ file: " << fileName << endl;
        exit(1);
    }

    string line, token;
    vector<int> face;

    getline( objStream, line );
    while( !objStream.eof() ) {
        trimString(line);
        if( line.length( ) > 0 && line.at(0) != '#' ) {
            istringstream lineStream( line );

            lineStream >> token;

            if (token == "v" ) {
                float x, y, z;
                lineStream >> x >> y >> z;
                points.push_back( QVector3D(x,y,z) );
            } else if (token == "vt" && loadTex) {
                // Process texture coordinate
                float s,t;
                lineStream >> s >> t;
                texCoords.push_back( QVector2D(s,t) );
            } else if (token == "vn" ) {
                float x, y, z;
                lineStream >> x >> y >> z;
                normals.push_back( QVector3D(x,y,z) );
            } else if (token == "f" ) {
                nFaces++;

                // Process face
                face.clear();
                size_t slash1, slash2;
                //int point, texCoord, normal;
                while( lineStream.good() ) {
                    string vertString;
                    lineStream >> vertString;
                    int pIndex = -1, nIndex = -1 , tcIndex = -1;

                    slash1 = vertString.find("/");
                    if( slash1 == string::npos ){
                        pIndex = atoi( vertString.c_str() ) - 1;
                    } else {
                        slash2 = vertString.find("/", slash1 + 1 );
                        pIndex = atoi( vertString.substr(0,slash1).c_str() )
                                        - 1;
                        if( slash2 > slash1 + 1 ) {
                                tcIndex =
                                        atoi( vertString.substr(slash1 + 1, slash2).c_str() )
                                        - 1;
                        }
                        nIndex =
                                atoi( vertString.substr(slash2 + 1,vertString.length()).c_str() )
                                - 1;
                    }
                    if( pIndex == -1 ) {
                        printf("Missing point index!!!");
                    } else {
                        face.push_back(pIndex);
                    }

                    if( loadTex && tcIndex != -1 && pIndex != tcIndex ) {
                        printf("Texture and point indices are not consistent.\n");
                    }
                    if ( nIndex != -1 && nIndex != pIndex ) {
                        printf("Normal and point indices are not consistent.\n");
                    }
                }
                // If number of edges in face is greater than 3,
                // decompose into triangles as a triangle fan.
                if( face.size() > 3 ) {
                    int v0 = face[0];
                    int v1 = face[1];
                    int v2 = face[2];
                    // First face
                    faces.push_back(v0);
                    faces.push_back(v1);
                    faces.push_back(v2);
                    for( unsigned int i = 3; i < face.size(); i++ ) {
                        v1 = v2;
                        v2 = face[i];
                        faces.push_back(v0);
                        faces.push_back(v1);
                        faces.push_back(v2);
                    }
                } else {
                    faces.push_back(face[0]);
                    faces.push_back(face[1]);
                    faces.push_back(face[2]);
                }
            }
        }
        getline( objStream, line );
    }

    objStream.close();

    if( normals.size() == 0 ) {
        generateAveragedNormals(points,normals,faces);
    }

    vector<QVector4D> tangents;
    if( genTang && texCoords.size() > 0 ) {
        generateTangents(points,normals,faces,texCoords,tangents);
    }

    if( reCenterMesh ) {
        center(points);
    }

    storeVBO(points, normals, texCoords, tangents, faces);

    cout << "Loaded mesh from: " << fileName << endl;
    cout << " " << points.size() << " points" << endl;
    cout << " " << nFaces << " faces" << endl;
    cout << " " << faces.size() / 3 << " triangles." << endl;
    cout << " " << normals.size() << " normals" << endl;
    cout << " " << tangents.size() << " tangents " << endl;
    cout << " " << texCoords.size() << " texture coordinates." << endl;
}

void VBOMesh::center( vector<QVector3D> & points ) {
    if( points.size() < 1) return;

    QVector3D maxPoint = points[0];
    QVector3D minPoint = points[0];

    // Find the AABB
    for( uint i = 0; i < points.size(); ++i ) {
        QVector3D & point = points[i];
        if( point.x() > maxPoint.x() ) maxPoint.setX(point.x());
        if( point.y() > maxPoint.y() ) maxPoint.setY(point.y());
        if( point.z() > maxPoint.z() ) maxPoint.setZ(point.z());
        if( point.x() < minPoint.x() ) minPoint.setX(point.x());
        if( point.y() < minPoint.y() ) minPoint.setY(point.y());
        if( point.z() < minPoint.z() ) minPoint.setZ(point.z());
    }

    // Center of the AABB
    QVector3D center = QVector3D( (maxPoint.x() + minPoint.x()) / 2.0f,
                        (maxPoint.y() + minPoint.y()) / 2.0f,
                        (maxPoint.z() + minPoint.z()) / 2.0f );

    // Translate center of the AABB to the origin
    for( uint i = 0; i < points.size(); ++i ) {
        QVector3D & point = points[i];
        point = point - center;
    }
}

void VBOMesh::generateAveragedNormals(
        const vector<QVector3D> & points,
        vector<QVector3D> & normals,
        const vector<unsigned int> & faces )
{
    for( uint i = 0; i < points.size(); i++ ) {
        normals.push_back(QVector3D());
    }

    for( uint i = 0; i < faces.size(); i += 3) {
        const QVector3D & p1 = points[faces[i]];
        const QVector3D & p2 = points[faces[i+1]];
        const QVector3D & p3 = points[faces[i+2]];

        QVector3D a = p2 - p1;
        QVector3D b = p3 - p1;
        QVector3D n = QVector3D::crossProduct(a,b).normalized();

        normals[faces[i]] += n;
        normals[faces[i+1]] += n;
        normals[faces[i+2]] += n;
    }

    for( uint i = 0; i < normals.size(); i++ ) {
        normals[i].normalize();
    }
}

void VBOMesh::generateTangents(
        const vector<QVector3D> & points,
        const vector<QVector3D> & normals,
        const vector<unsigned int> & faces,
        const vector<QVector2D> & texCoords,
        vector<QVector4D> & tangents)
{
    vector<QVector3D> tan1Accum;
    vector<QVector3D> tan2Accum;

    for( uint i = 0; i < points.size(); i++ ) {
        tan1Accum.push_back(QVector3D());
        tan2Accum.push_back(QVector3D());
        tangents.push_back(QVector4D());
    }

    // Compute the tangent vector
    for( uint i = 0; i < faces.size(); i += 3 )
    {
        const QVector3D &p1 = points[faces[i]];
        const QVector3D &p2 = points[faces[i+1]];
        const QVector3D &p3 = points[faces[i+2]];

        const QVector2D &tc1 = texCoords[faces[i]];
        const QVector2D &tc2 = texCoords[faces[i+1]];
        const QVector2D &tc3 = texCoords[faces[i+2]];

        QVector3D q1 = p2 - p1;
        QVector3D q2 = p3 - p1;
        float s1 = tc2.x() - tc1.x(), s2 = tc3.x() - tc1.x();
        float t1 = tc2.y() - tc1.y(), t2 = tc3.y() - tc1.y();
        float r = 1.0f / (s1 * t2 - s2 * t1);
        QVector3D tan1( (t2*q1.x() - t1*q2.x()) * r,
                   (t2*q1.y() - t1*q2.y()) * r,
                   (t2*q1.z() - t1*q2.z()) * r);
        QVector3D tan2( (s1*q2.x() - s2*q1.x()) * r,
                   (s1*q2.y() - s2*q1.y()) * r,
                   (s1*q2.z() - s2*q1.z()) * r);
        tan1Accum[faces[i]] += tan1;
        tan1Accum[faces[i+1]] += tan1;
        tan1Accum[faces[i+2]] += tan1;
        tan2Accum[faces[i]] += tan2;
        tan2Accum[faces[i+1]] += tan2;
        tan2Accum[faces[i+2]] += tan2;
    }

    for( uint i = 0; i < points.size(); ++i )
    {
        const QVector3D &n = normals[i];
        QVector3D &t1 = tan1Accum[i];
        QVector3D &t2 = tan2Accum[i];

        // Gram-Schmidt orthogonalize
        tangents[i] = QVector4D(( t1 - (QVector3D::dotProduct(n,t1) * n) ).normalized(), 0.0f).normalized();
        // Store handedness in w
        tangents[i].setW( (QVector3D::dotProduct(QVector3D::crossProduct(n,t1), t2 ) < 0.0f) ? -1.0f : 1.0f );
    }
    tan1Accum.clear();
    tan2Accum.clear();
}

void VBOMesh::storeVBO( const vector<QVector3D> & points,
                        const vector<QVector3D> & normals,
                        const vector<QVector2D> &texCoords,
                        const vector<QVector4D> &tangents,
                        const vector<unsigned int> &elements )
{
    nVerts = (unsigned int) points.size();
    nFaces = (unsigned int) elements.size() / 3;

    v = new float[3 * nVerts];
    n = new float[3 * nVerts];

    if(texCoords.size() > 0) {
        tc = new float[ 2 * nVerts];
        if( tangents.size() > 0 )
            tang = new float[4*nVerts];
    }

    el = new unsigned int[elements.size()];

    int idx = 0, tcIdx = 0, tangIdx = 0;
    for( unsigned int i = 0; i < nVerts; ++i )
    {
        v[idx] = points[i].x();
        v[idx+1] = points[i].y();
        v[idx+2] = points[i].z();
        n[idx] = normals[i].x();
        n[idx+1] = normals[i].y();
        n[idx+2] = normals[i].z();
        idx += 3;
        if( tc != NULL ) {
            tc[tcIdx] = texCoords[i].x();
            tc[tcIdx+1] = texCoords[i].y();
            tcIdx += 2;
        }
        if( tang != NULL ) {
            tang[tangIdx] = tangents[i].x();
            tang[tangIdx+1] = tangents[i].y();
            tang[tangIdx+2] = tangents[i].z();
            tang[tangIdx+3] = tangents[i].w();
            tangIdx += 4;
        }
    }
    for( unsigned int i = 0; i < elements.size(); ++i )
    {
        el[i] = elements[i];
    }
}

void VBOMesh::trimString( string & str ) {
    const char * whiteSpace = " \t\n\r";
    size_t location;
    location = str.find_first_not_of(whiteSpace);
    str.erase(0,location);
    location = str.find_last_not_of(whiteSpace);
    str.erase(location + 1);
}

float *VBOMesh::getv()
{
    return v;
}

unsigned int VBOMesh::getnVerts()
{
    return nVerts;
}

float *VBOMesh::getn()
{
    return n;
}

float *VBOMesh::gettc()
{
    return tc;
}

unsigned int *VBOMesh::getelems()
{
    return el;
}

unsigned int VBOMesh::getnFaces()
{
    return nFaces;
}

float *VBOMesh::gettang()
{
    return tang;
}
