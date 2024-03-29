#include "lp_openmesh.h"
#include "lp_renderercam.h"

#include <QOpenGLContext>
#include <QOpenGLShaderProgram>
#include <QOpenGLExtraFunctions>

LP_OpenMeshImpl::LP_OpenMeshImpl(LP_Object parent) : LP_GeometryImpl(parent)
  ,mVBO(nullptr)
  ,mIndices(nullptr)
{

}


LP_OpenMeshImpl::~LP_OpenMeshImpl()
{
    Q_ASSERT(mPrograms.empty());
    Q_ASSERT(!mProgramBoundary);
    Q_ASSERT(!mVBO);
    Q_ASSERT(!mIndices);
}

MyMesh LP_OpenMeshImpl::Mesh() const
{
    return mMesh;
}

void LP_OpenMeshImpl::SetMesh(const MyMesh &mesh)
{
    mMesh = mesh;
}

void LP_OpenMeshImpl::_Dump(QDebug &debug)
{
    LP_GeometryImpl::_Dump(debug);
    debug.nospace() << "#V : " << mMesh->n_vertices() << ", "
                    << "#E : " << mMesh->n_edges() << ", "
                    << "#F : " << mMesh->n_faces()
                    << "\n";
}

void LP_OpenMeshImpl::Draw(QOpenGLContext *ctx, QSurface *surf, QOpenGLFramebufferObject *fbo, const LP_RendererCam &cam, QVariant &option)
{
    Q_UNUSED(surf)
    Q_UNUSED(fbo)

    auto proj = cam->ProjectionMatrix();
    auto view = cam->ViewMatrix();
    auto f = ctx->extraFunctions();

    auto it = mPrograms.find(option.toString());
    if ( it == mPrograms.cend()){
        return;
    }
    auto prog = it.value();

    prog->bind();
    prog->setUniformValue("m4_mvp", proj*view );
    prog->setUniformValue("m4_view", view );
    prog->setUniformValue("m3_normal", view.normalMatrix());
    prog->enableAttributeArray("a_pos");
    prog->enableAttributeArray("a_norm");
    prog->enableAttributeArray("a_color");

    mVBO->bind();
    mIndices->bind();
    prog->setAttributeBuffer("a_pos",GL_FLOAT, 0, 3, 36);
    prog->setAttributeBuffer("a_norm",GL_FLOAT, 12, 3, 36);
    prog->setAttributeBuffer("a_color",GL_FLOAT, 24, 3, 36);

    f->glDrawElements(GL_TRIANGLES, GLsizei(mStrides[0]), GL_UNSIGNED_INT, nullptr);

    mVBO->release();
    mIndices->release();

    prog->disableAttributeArray("a_pos");
    prog->disableAttributeArray("a_norm");
    prog->disableAttributeArray("a_color");
    prog->release();

    f->glLineWidth(2.0f);
    mProgramBoundary->bind();
    mProgramBoundary->setUniformValue("m4_mvp", proj*view );
    mProgramBoundary->setUniformValue("u_color", QVector3D(0.0,1.0,1.0));

    mVBO->bind();
    mIndices->bind();

    mProgramBoundary->enableAttributeArray("a_pos");
    mProgramBoundary->enableAttributeArray("a_norm");
    mProgramBoundary->setAttributeBuffer("a_pos",GL_FLOAT, 0, 3, 36);
    mProgramBoundary->setAttributeBuffer("a_norm",GL_FLOAT, 12, 3, 36);

    f->glDrawElements(  GL_LINES,
                        GLsizei(mStrides[1]-mStrides[0]),
                        GL_UNSIGNED_INT,
                        (void*)(mStrides[0]*sizeof(uint)));

    mProgramBoundary->disableAttributeArray("a_pos");
    mProgramBoundary->disableAttributeArray("a_norm");

    mVBO->release();
    mIndices->release();

    mProgramBoundary->release();
}


bool LP_OpenMeshImpl::DrawSetup(QOpenGLContext *ctx, QSurface *surf, QVariant &option)
{
    QWriteLocker locker(&mLock);
    ctx->makeCurrent(surf);

    std::string vsh, fsh;
    if ( option.toString() == "Normal"){
        vsh =
                "attribute vec3 a_pos;\n"
                "attribute vec3 a_norm;\n"
                "uniform mat4 m4_mvp;\n"
                "varying vec3 normal;\n"
                "void main(){\n"
                "   normal = a_norm;\n"
                "   gl_Position = m4_mvp*vec4(a_pos, 1.0);\n"
                "}";
        fsh =
                "varying vec3 normal;\n"
                "void main(){\n"
                "   vec3 n = 0.5 * ( vec3(1.0) + normalize(normal));\n"
                "   gl_FragColor = vec4(n,1.0);\n"
                "}";
    }else{
        vsh =
                "attribute vec3 a_pos;\n"
                "attribute vec3 a_norm;\n"
                "attribute vec3 a_color;\n"
                "uniform mat4 m4_view;\n"
                "uniform mat4 m4_mvp;\n"
                "uniform mat3 m3_normal;\n"
                "varying vec3 normal;\n"
                "varying vec3 pos;\n"
                "varying vec3 vcolor;\n"
                "void main(){\n"
                "   pos = vec3( m4_view * vec4(a_pos, 1.0));\n"
                "   normal = m3_normal * a_norm;\n"
                "   vcolor = a_color;\n"
                "   gl_Position = m4_mvp*vec4(a_pos, 1.0);\n"
                "}";
        fsh =
                "varying vec3 normal;\n"
                "varying vec3 pos;\n"
                "varying vec3 vcolor;\n"
                "void main(){\n"
                "   vec3 lightPos = vec3(0.0, 1000.0, 0.0);\n"
                "   vec3 viewDir = normalize( - pos);\n"
                "   vec3 lightDir = normalize(lightPos - pos);\n"
                "   vec3 H = normalize(viewDir + lightDir);\n"
                "   vec3 N = normalize(normal);\n"
                "   vec3 ambi = vcolor;\n"
                "   float Kd = max(dot(H, N), 0.0);\n"
                "   vec3 diff = Kd * vec3(0.8, 0.8, 0.8);\n"
                "   vec3 color = ambi + Kd * diff;\n"
//                "   float Ks = pow( Kd, 80.0 );\n"
//                "   vec3 spec = Ks * vec3(1.0, 1.0, 1.0);\n"
//                "   color += spec;\n"
                "   gl_FragColor = vec4(color,1.0);\n"
                "}";
    }

    if ( !mPrograms.contains(option.toString())){
        auto prog = new QOpenGLShaderProgram;
        prog->addShaderFromSourceCode(QOpenGLShader::Vertex,vsh.c_str());
        prog->addShaderFromSourceCode(QOpenGLShader::Fragment,fsh.data());
        if ( !prog->create() || !prog->link()){
            qCritical() << prog->log();
        }
        mPrograms[option.toString()] = prog;
    }


    if ( mGLInitialized ){
        return true;
    }

    const char vsh2[] =
            "attribute vec3 a_pos;\n"
            "uniform mat4 m4_mvp;\n"
            "void main(){\n"\
            "   gl_Position = m4_mvp*vec4(a_pos, 1.0);\n"
            "   gl_PointSize = 15.0;\n"
            "}";
    const char fsh2[] =
            "precision mediump float;\n"
            "uniform vec3 u_color;\n"
            "void main(){\n"
            "   gl_FragColor = vec4(u_color,1.0);\n"
            "}";

    if ( !mProgramBoundary ){
        mProgramBoundary = std::make_shared<QOpenGLShaderProgram>();
        mProgramBoundary->addShaderFromSourceCode(QOpenGLShader::Vertex,vsh2);
        mProgramBoundary->addShaderFromSourceCode(QOpenGLShader::Fragment,fsh2);
        if ( !mProgramBoundary->create() || !mProgramBoundary->link()){
            qCritical() << mProgramBoundary->log();
        }
    }

    std::vector<uint> indices_boundary;
    for ( auto &e : mMesh->edges()){
        if ( e.is_boundary()){
            indices_boundary.emplace_back( e.v0().idx());
            indices_boundary.emplace_back( e.v1().idx());
        }
    }

    mVBO = new QOpenGLBuffer(QOpenGLBuffer::VertexBuffer);
    mVBO->setUsagePattern(QOpenGLBuffer::StreamDraw);
    mVBO->create();

    auto nVs = mMesh->n_vertices();
    const int _a = int( nVs * (sizeof(OpMesh::Point)+
                               sizeof(OpMesh::Normal)+
                               sizeof(OpMesh::Point)));
    mVBO->bind();
    mVBO->allocate( _a );
    //mVBO->allocate( m->points(), int( m->n_vertices() * sizeof(*m->points())));
    auto ptr = static_cast<OpMesh::Point*>(mVBO->mapRange(0, _a,
                                                          QOpenGLBuffer::RangeWrite |
                                                          QOpenGLBuffer::RangeInvalidateBuffer));
    auto pptr = mMesh->points();
    auto nptr = mMesh->vertex_normals();
    auto cptr = mMesh->vertex_colors();

    QVector3D c;
    const float _inv = 1.0f / 255.0f;
    qDebug() << "Size of : " << sizeof(OpMesh::Point) << " , color - " << sizeof(OpMesh::Color);
    for ( size_t i=0; i<nVs; ++i, ++ptr, ++pptr, ++nptr, ++cptr ){
        memcpy(ptr, pptr, sizeof(OpMesh::Point));
        memcpy(++ptr, nptr, sizeof(OpMesh::Normal));

        const uchar *_p = cptr->data();

        c.setX(_p[0] * _inv);
        c.setY(_p[1] * _inv);
        c.setZ(_p[2] * _inv);

        memcpy(++ptr, &c[0], sizeof(OpMesh::Point));

        if ( mBBmin.x() > (*pptr)[0]){
            mBBmin.setX((*pptr)[0]);
        }else if ( mBBmax.x() < (*pptr)[0]){
            mBBmax.setX((*pptr)[0]);
        }

        if ( mBBmin.y() > (*pptr)[1]){
            mBBmin.setY((*pptr)[1]);
        }else if ( mBBmax.y() < (*pptr)[1]){
            mBBmax.setY((*pptr)[1]);
        }

        if ( mBBmin.z() > (*pptr)[2]){
            mBBmin.setZ((*pptr)[2]);
        }else if ( mBBmax.z() < (*pptr)[2]){
            mBBmax.setZ((*pptr)[2]);
        }

        //qDebug() << QString("%1, %2, %3").arg((*nptr)[0]).arg((*nptr)[1]).arg((*nptr)[2]);
    }
    mVBO->unmap();
    mVBO->release();

    //===============================================================
    //Indice buffer
    //TODO: Not save for non-triangular mesh
    std::vector<uint> indices(mMesh->n_faces()*3);
    auto i_it = indices.begin();
    for ( const auto &f : mMesh->faces()){
        for ( const auto &v : f.vertices()){
            //indices.emplace_back(v.idx());
            (*i_it++) = v.idx();
        }
    }

    mIndices = new QOpenGLBuffer(QOpenGLBuffer::IndexBuffer);
    mIndices->setUsagePattern(QOpenGLBuffer::StaticDraw);

    const int a_ = int( indices.size() + indices_boundary.size()) * sizeof(indices[0]);
    mIndices->create();
    mIndices->bind();
    mIndices->allocate( a_ );
    auto iptr = static_cast<uint*>(mIndices->mapRange(0, a_,
                                                      QOpenGLBuffer::RangeWrite |
                                                      QOpenGLBuffer::RangeInvalidateBuffer));
    memcpy(iptr, indices.data(), indices.size() * sizeof(indices[0]));
    memcpy(iptr+indices.size(), indices_boundary.data(), indices_boundary.size() * sizeof(indices_boundary[0]));
    mIndices->unmap();
    mIndices->release();

    ctx->doneCurrent();

    mStrides[0] = indices.size();
    mStrides[1] = indices.size() + indices_boundary.size();

    mGLInitialized = true;
    return true;
}

bool LP_OpenMeshImpl::DrawCleanUp(QOpenGLContext *ctx, QSurface *surf)
{
    QWriteLocker locker(&mLock);
    ctx->makeCurrent(surf);

    if ( mVBO ){
        mVBO->release();
        delete mVBO;
        mVBO = nullptr;
    }

    if ( mIndices ){
        mIndices->release();
        delete mIndices;
        mIndices = nullptr;
    }

    for ( auto p : mPrograms ){
        p->release();
        delete p;
    }
    mPrograms.clear();

    if ( mProgramBoundary ){
        mProgramBoundary->release();
        mProgramBoundary.reset();
    }

    ctx->doneCurrent();
    mGLInitialized = false; //Unitialized

    return true;
}


void LP_OpenMeshImpl::DrawSelect(QOpenGLContext *ctx, QSurface *surf, QOpenGLFramebufferObject *fbo, QOpenGLShaderProgram *prog, const LP_RendererCam &cam)
{
    Q_UNUSED(surf)
    Q_UNUSED(fbo)
    Q_UNUSED(cam)

    auto f = ctx->extraFunctions();

    mVBO->bind();
    mIndices->bind();
    prog->enableAttributeArray("a_pos");
    prog->setAttributeBuffer("a_pos",GL_FLOAT, 0, 3, 36);

    f->glDrawElements(GL_TRIANGLES, GLsizei(mStrides[0]), GL_UNSIGNED_INT, nullptr);

    prog->disableAttributeArray("a_pos");
    mVBO->release();
    mIndices->release();
}
