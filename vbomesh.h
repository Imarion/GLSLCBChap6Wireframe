#ifndef VBOMESH_H
#define VBOMESH_H


#include <vector>
using std::vector;

#include <string>
using std::string;

#include <QVector2D>
#include <QVector3D>
#include <QVector4D>

class VBOMesh
{
private:   
    unsigned int nFaces;

    // Vertices
    float *v;
    unsigned int nVerts;

    // Normals
    float *n;

    // Tex coords
    float *tc;

    // Elements
    unsigned int *el;

    float *tang;

    bool reCenterMesh, loadTex, genTang;

    void trimString( string & str );
    void storeVBO( const vector<QVector3D> & points,
                            const vector<QVector3D> & normals,
                            const vector<QVector2D> &texCoords,
                            const vector<QVector4D> &tangents,
                            const vector<unsigned int> &elements );
    void generateAveragedNormals(
            const vector<QVector3D> & points,
            vector<QVector3D> & normals,
            const vector<unsigned int> & faces );
    void generateTangents(
            const vector<QVector3D> & points,
            const vector<QVector3D> & normals,
            const vector<unsigned int> & faces,
            const vector<QVector2D> & texCoords,
            vector<QVector4D> & tangents);
    void center(vector<QVector3D> &);

public:
    VBOMesh( const char * fileName, bool reCenterMesh = false, bool loadTc = false, bool genTangents = false );
    ~VBOMesh();

    void render() const;

    void loadOBJ( const char * fileName );

    float *getv();
    unsigned int getnVerts();
    float *getn();
    float *gettc();
    unsigned int *getelems();
    unsigned int  getnFaces();
    float *gettang();
};

#endif // VBOMESH_H
