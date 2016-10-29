#include "Wireframe.h"

#include <QtGlobal>

#include <QDebug>
#include <QFile>
#include <QImage>
#include <QTime>

#include <QVector2D>
#include <QVector3D>
#include <QMatrix4x4>

#include <cmath>
#include <cstring>

MyWindow::~MyWindow()
{
    if (mProgram != 0) delete mProgram;
}

MyWindow::MyWindow()
    : mProgram(0), currentTimeMs(0), currentTimeS(0), angle(1.74533f), tPrev(0.0f), rotSpeed(M_PI / 8.0f)
{
    setSurfaceType(QWindow::OpenGLSurface);
    setFlags(Qt::Window | Qt::WindowSystemMenuHint | Qt::WindowTitleHint | Qt::WindowMinMaxButtonsHint | Qt::WindowCloseButtonHint);

    QSurfaceFormat format;
    format.setDepthBufferSize(24);
    format.setMajorVersion(4);
    format.setMinorVersion(3);
    format.setSamples(4);
    format.setProfile(QSurfaceFormat::CoreProfile);
    setFormat(format);
    create();

    resize(800, 600);

    mContext = new QOpenGLContext(this);
    mContext->setFormat(format);
    mContext->create();

    mContext->makeCurrent( this );

    mFuncs = mContext->versionFunctions<QOpenGLFunctions_4_3_Core>();
    if ( !mFuncs )
    {
        qWarning( "Could not obtain OpenGL versions object" );
        exit( 1 );
    }
    if (mFuncs->initializeOpenGLFunctions() == GL_FALSE)
    {
        qWarning( "Could not initialize core open GL functions" );
        exit( 1 );
    }

    initializeOpenGLFunctions();

    QTimer *repaintTimer = new QTimer(this);
    connect(repaintTimer, &QTimer::timeout, this, &MyWindow::render);
    repaintTimer->start(1000/60);

    QTimer *elapsedTimer = new QTimer(this);
    connect(elapsedTimer, &QTimer::timeout, this, &MyWindow::modCurTime);
    elapsedTimer->start(1);       
}

void MyWindow::modCurTime()
{
    currentTimeMs++;
    currentTimeS=currentTimeMs/1000.0f;
}

void MyWindow::initialize()
{
    mFuncs->glGenVertexArrays(1, &mVAO);
    mFuncs->glBindVertexArray(mVAO);

    CreateVertexBuffer();
    initShaders();
    initMatrices();

    PrepareTexture(GL_TEXTURE0, GL_TEXTURE_2D, "../Media/ogre_diffuse.png", true);
    PrepareTexture(GL_TEXTURE1, GL_TEXTURE_2D, "../Media/ogre_normalmap.png", true);

    glFrontFace(GL_CCW);
    glEnable(GL_DEPTH_TEST);
}

void MyWindow::CreateVertexBuffer()
{
    mOgre = new VBOMesh("../media/bs_ears.obj", false, true, true);

    // Create and populate the buffer objects
    unsigned int handle[5];
    glGenBuffers(5, handle);

    glBindBuffer(GL_ARRAY_BUFFER, handle[0]);
    glBufferData(GL_ARRAY_BUFFER, (3 * mOgre->getnVerts()) * sizeof(float), mOgre->getv(), GL_STATIC_DRAW);

    glBindBuffer(GL_ARRAY_BUFFER, handle[1]);
    glBufferData(GL_ARRAY_BUFFER, (3 * mOgre->getnVerts()) * sizeof(float), mOgre->getn(), GL_STATIC_DRAW);

    glBindBuffer(GL_ARRAY_BUFFER, handle[2]);
    glBufferData(GL_ARRAY_BUFFER, (2 * mOgre->getnVerts()) * sizeof(float), mOgre->gettc(), GL_STATIC_DRAW);

    glBindBuffer(GL_ARRAY_BUFFER, handle[3]);
    glBufferData(GL_ARRAY_BUFFER, (4 * mOgre->getnVerts()) * sizeof(float), mOgre->gettang(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, handle[4]);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, 3 * mOgre->getnFaces() * sizeof(unsigned int), mOgre->getelems(), GL_STATIC_DRAW);

    // Setup the VAO
    // Vertex positions
    mFuncs->glBindVertexBuffer(0, handle[0], 0, sizeof(GLfloat) * 3);
    mFuncs->glVertexAttribFormat(0, 3, GL_FLOAT, GL_FALSE, 0);
    mFuncs->glVertexAttribBinding(0, 0);

    // Vertex normals
    mFuncs->glBindVertexBuffer(1, handle[1], 0, sizeof(GLfloat) * 3);
    mFuncs->glVertexAttribFormat(1, 3, GL_FLOAT, GL_FALSE, 0);
    mFuncs->glVertexAttribBinding(1, 1);

    // Vertex texure coordinates
    mFuncs->glBindVertexBuffer(2, handle[2], 0, sizeof(GLfloat) * 2);
    mFuncs->glVertexAttribFormat(2, 2, GL_FLOAT, GL_FALSE, 0);
    mFuncs->glVertexAttribBinding(2, 2);

    // Vertex tangents
    mFuncs->glBindVertexBuffer(3, handle[3], 0, sizeof(GLfloat) * 4);
    mFuncs->glVertexAttribFormat(3, 4, GL_FLOAT, GL_FALSE, 0);
    mFuncs->glVertexAttribBinding(3, 3);

    // Indices
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, handle[4]);

    mFuncs->glBindVertexArray(0);
}

void MyWindow::initMatrices()
{
    ViewMatrix.lookAt(QVector3D(-1.0f,0.25f,2.0f), QVector3D(0.0f, 0.0f, 0.0f), QVector3D(0.0f, 1.0f, 0.0f));
}

void MyWindow::resizeEvent(QResizeEvent *)
{
    mUpdateSize = true;

    ProjectionMatrix.setToIdentity();
    //ProjectionMatrix.perspective(70.0f, (float)this->width()/(float)this->height(), 0.3f, 100.0f);
    float c = 2.0f;
    ProjectionMatrix.ortho(-0.4f * c, 0.4f * c, -0.3f * c, 0.3f * c, 0.1f, 100.0f);

    float w2 = (float) this->width()  / 2.0f;
    float h2 = (float) this->height() / 2.0f;

    ViewPortMatrix = QMatrix4x4(w2,   0.0f, 0.0f, 0.0f,
                                0.0f, h2,   0.0f, 0.0f,
                                0.0f, 0.0f, 1.0f, 0.0f,
                                w2,   h2,   0.0f, 1.0f);
}

void MyWindow::render()
{
    if(!isVisible() || !isExposed())
        return;

    if (!mContext->makeCurrent(this))
        return;

    static bool initialized = false;
    if (!initialized) {
        initialize();
        initialized = true;
    }

    if (mUpdateSize) {
        glViewport(0, 0, size().width(), size().height());
        mUpdateSize = false;
    }

    float deltaT = currentTimeS - tPrev;
    if(tPrev == 0.0f) deltaT = 0.0f;
    tPrev = currentTimeS;
    angle += 0.25f * deltaT;
    if (angle > TwoPI) angle -= TwoPI;

    static float EvolvingVal = 0;
    EvolvingVal += 0.1f;

    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    //QMatrix4x4 RotationMatrix;
    //RotationMatrix.rotate(EvolvingVal, QVector3D(0.1f, 0.0f, 0.1f));
    //ModelMatrix.rotate(0.3f, QVector3D(0.1f, 0.0f, 0.1f));

    mFuncs->glBindVertexArray(mVAO);

    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);
    glEnableVertexAttribArray(3);

    mProgram->bind();
    {
        QVector4D worldLight = QVector4D(10.0f * cos(angle),1.0f,10.0f * sin(angle),1.0f);
        mProgram->setUniformValue("Light.Position", ViewMatrix * worldLight );
        mProgram->setUniformValue("Light.Intensity", 0.9f, 0.9f, 0.9f);

        mProgram->setUniformValue("Material.Kd", 0.9f, 0.9f, 0.9f);
        mProgram->setUniformValue("Material.Ks", 0.2f, 0.2f, 0.2f);
        mProgram->setUniformValue("Material.Ka", 0.1f, 0.1f, 0.1f);
        mProgram->setUniformValue("Material.Shininess", 1.0f);

        QMatrix4x4 mv1 = ViewMatrix * ModelMatrix;
        mProgram->setUniformValue("ModelViewMatrix", mv1);
        mProgram->setUniformValue("NormalMatrix", mv1.normalMatrix());
        mProgram->setUniformValue("MVP", ProjectionMatrix * mv1);

        mProgram->setUniformValue("ViewportMatrix", ViewPortMatrix);

        glDrawElements(GL_TRIANGLES, 3 * mOgre->getnFaces(), GL_UNSIGNED_INT, ((GLubyte *)NULL + (0)));

        glDisableVertexAttribArray(0);
        glDisableVertexAttribArray(1);
        glDisableVertexAttribArray(2);
        glDisableVertexAttribArray(3);
    }
    mProgram->release();

    mContext->swapBuffers(this);
}

void MyWindow::initShaders()
{
    QOpenGLShader vShader(QOpenGLShader::Vertex);
    QOpenGLShader gShader(QOpenGLShader::Geometry);
    QOpenGLShader fShader(QOpenGLShader::Fragment);
    QFile         shaderFile;
    QByteArray    shaderSource;

    shaderFile.setFileName(":/vshader.txt");
    shaderFile.open(QIODevice::ReadOnly);
    shaderSource = shaderFile.readAll();
    shaderFile.close();
    qDebug() << "vertex   compile: " << vShader.compileSourceCode(shaderSource);

    shaderFile.setFileName(":/gshader.txt");
    shaderFile.open(QIODevice::ReadOnly);
    shaderSource = shaderFile.readAll();
    shaderFile.close();
    qDebug() << "geometry compile: " << gShader.compileSourceCode(shaderSource);

    shaderFile.setFileName(":/fshader.txt");
    shaderFile.open(QIODevice::ReadOnly);
    shaderSource = shaderFile.readAll();
    shaderFile.close();
    qDebug() << "frag     compile: " << fShader.compileSourceCode(shaderSource);

    mProgram = new (QOpenGLShaderProgram);
    mProgram->addShader(&vShader);
    mProgram->addShader(&gShader);
    mProgram->addShader(&fShader);
    qDebug() << "shader link tree: " << mProgram->link();
}

void MyWindow::PrepareTexture(GLenum TextureUnit, GLenum TextureTarget, const QString& FileName, bool flip)
{
    QImage TexImg;

    if (!TexImg.load(FileName)) qDebug() << "Erreur chargement texture " << FileName;
    if (flip==true) TexImg=TexImg.mirrored();

    glActiveTexture(TextureUnit);
    GLuint TexObject;
    glGenTextures(1, &TexObject);
    glBindTexture(TextureTarget, TexObject);
    mFuncs->glTexStorage2D(TextureTarget, 1, GL_RGBA8, TexImg.width(), TexImg.height());
    mFuncs->glTexSubImage2D(TextureTarget, 0, 0, 0, TexImg.width(), TexImg.height(), GL_BGRA, GL_UNSIGNED_BYTE, TexImg.bits());
    //glTexImage2D(TextureTarget, 0, GL_RGB, TexImg.width(), TexImg.height(), 0, GL_BGRA, GL_UNSIGNED_BYTE, TexImg.bits());
    glTexParameteri(TextureTarget, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(TextureTarget, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
}

void MyWindow::keyPressEvent(QKeyEvent *keyEvent)
{
    switch(keyEvent->key())
    {
        case Qt::Key_P:
            break;
        case Qt::Key_Up:
            break;
        case Qt::Key_Down:
            break;
        case Qt::Key_Left:
            break;
        case Qt::Key_Right:
            break;
        case Qt::Key_Delete:
            break;
        case Qt::Key_PageDown:
            break;
        case Qt::Key_Home:
            break;
        case Qt::Key_Z:
            break;
        case Qt::Key_Q:
            break;
        case Qt::Key_S:
            break;
        case Qt::Key_D:
            break;
        case Qt::Key_A:
            break;
        case Qt::Key_E:
            break;
        default:
            break;
    }
}

void MyWindow::printMatrix(const QMatrix4x4& mat)
{
    const float *locMat = mat.transposed().constData();

    for (int i=0; i<4; i++)
    {
        qDebug() << locMat[i*4] << " " << locMat[i*4+1] << " " << locMat[i*4+2] << " " << locMat[i*4+3];
    }
}
