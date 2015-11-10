#include <GL/glew.h>

#include "shaderfactory.h"
#include "viewer.h"
#include "global.h"
#include "staticfunctions.h"

#include <QInputDialog>
#include <QProgressDialog>
#include <QLabel>

Viewer::Viewer(QWidget *parent) :
  QGLViewer(parent)
{
  setMouseTracking(true);

  m_memSize = 1000; // size in MB

  m_clipPlanes = new ClipPlanes();

  m_slcBuffer = 0;
  m_rboId = 0;
  m_slcTex[0] = 0;
  m_slcTex[1] = 0;

  m_depthShader = 0;
  m_finalPointShader = 0;
  m_blurShader = 0;
  m_sliceShader = 0;
  m_rcShader = 0;
  m_eeShader = 0;

  m_tagTex = 0;
  m_maskTex = 0;
  m_dataTex = 0;
  m_lutTex = 0;
  m_corner = Vec(0,0,0);
  m_vsize = Vec(1,1,1);
  m_sslevel = 1;

  m_skipLayers = 0;

  m_glewInitdone = false;

  init();

  setMinimumSize(100, 100);

  QTimer::singleShot(2000, this, SLOT(GlewInit()));

  setTextureMemorySize();
}

void
Viewer::setTextureMemorySize()
{
  QString homePath = QDir::homePath();
  QFileInfo settingsFile(homePath, ".drishti.xml");

  if (! settingsFile.exists())
    return;

  QString flnm = settingsFile.absoluteFilePath();  


  QDomDocument document;
  QFile f(flnm.toLatin1().data());
  if (f.open(QIODevice::ReadOnly))
    {
      document.setContent(&f);
      f.close();
    }

  QDomElement main = document.documentElement();
  QDomNodeList dlist = main.childNodes();
  for(int i=0; i<dlist.count(); i++)
    {
      if (dlist.at(i).nodeName() == "texturememory")
	{
	  QString str = dlist.at(i).toElement().text();
	  m_memSize = str.toInt();
	  return;
	}
    }
}

void
Viewer::GlewInit()
{
  if (glewInit() != GLEW_OK)
    QMessageBox::information(0, "Glew",
			     "Failed to initialise glew");
  
  if (glewGetExtension("GL_ARB_fragment_shader")      != GL_TRUE ||
      glewGetExtension("GL_ARB_vertex_shader")        != GL_TRUE ||
      glewGetExtension("GL_ARB_shader_objects")       != GL_TRUE ||
      glewGetExtension("GL_ARB_shading_language_100") != GL_TRUE)
    QMessageBox::information(0, "Glew",
				 "Driver does not support OpenGL Shading Language.");
  
  
  if (glewGetExtension("GL_EXT_framebuffer_object") != GL_TRUE)
      QMessageBox::information(0, "Glew", 
			       "Driver does not support Framebuffer Objects (GL_EXT_framebuffer_object)");

  m_glewInitdone = true;

  createShaders();
  createFBO();

  glPixelStorei(GL_PACK_ALIGNMENT, 1);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

  glGetIntegerv(GL_MAX_3D_TEXTURE_SIZE, &m_max3DTexSize);
}

void
Viewer::init()
{
  m_skipLayers = 0;
  m_fullRender = false;
  m_dragMode = true;

  m_bitmask.clear();

  m_paintHit = false;
  m_carveHit = false;
  m_target = Vec(-1,-1,-1);

  m_Dcg = 0;
  m_Wcg = 0;
  m_Hcg = 0;
  m_Dmcg = 0;
  m_Wmcg = 0;
  m_Hmcg = 0;
  m_Dswcg = 0;
  m_Wswcg = 0;
  m_Hswcg = 0;

  m_fibers = 0;

  m_depth = 0;
  m_width = 0;
  m_height = 0;

  m_currSlice = 0;
  m_currSliceType = 0;

  m_maskPtr = 0;
  m_volPtr = 0;
  m_pointSkip = 5;
  m_pointSize = 5;
  m_pointScaling = 5;
  m_dzScale = 3.0;

  m_voxChoice = 0;
  m_voxels.clear();
  m_clipVoxels.clear();

  m_showSlices = true;
  m_dslice = 0;
  m_wslice = 0;
  m_hslice = 0;
  m_dvoxels.clear();
  m_wvoxels.clear();
  m_hvoxels.clear();

  m_minDSlice = 0;
  m_maxDSlice = 0;
  m_minWSlice = 0;
  m_maxWSlice = 0;
  m_minHSlice = 0;
  m_maxHSlice = 0;  

  m_paintedTags.clear();
  m_paintedTags << -1;

  m_curveTags.clear();
  m_curveTags << -1;

  m_fiberTags.clear();
  m_fiberTags << -1;

  m_showBox = true;

  m_clipPlanes->clear();

  if (m_rcShader)
    glDeleteObjectARB(m_rcShader);
  m_rcShader = 0;

  if (m_eeShader)
    glDeleteObjectARB(m_eeShader);
  m_eeShader = 0;

  if (m_depthShader)
    glDeleteObjectARB(m_depthShader);
  m_depthShader = 0;

  if (m_blurShader)
    glDeleteObjectARB(m_blurShader);
  m_blurShader = 0;

  if (m_sliceShader)
    glDeleteObjectARB(m_sliceShader);
  m_sliceShader = 0;

  if (m_finalPointShader)
    glDeleteObjectARB(m_finalPointShader);
  m_finalPointShader = 0;

  if (m_slcBuffer) glDeleteFramebuffers(1, &m_slcBuffer);
  if (m_rboId) glDeleteRenderbuffers(1, &m_rboId);
  if (m_slcTex[0]) glDeleteTextures(2, m_slcTex);
  m_slcBuffer = 0;
  m_rboId = 0;
  m_slcTex[0] = m_slcTex[1] = 0;

  if (m_dataTex) glDeleteTextures(1, &m_dataTex);
  m_dataTex = 0;

  if (m_maskTex) glDeleteTextures(1, &m_maskTex);
  m_maskTex = 0;

  if (m_tagTex) glDeleteTextures(1, &m_tagTex);
  m_tagTex = 0;

  if (m_lutTex) glDeleteTextures(1, &m_lutTex);
  m_lutTex = 0;

  m_corner = Vec(0,0,0);
  m_vsize = Vec(1,1,1);
  m_sslevel = 1;

}

void
Viewer::resizeGL(int width, int height)
{
  QGLViewer::resizeGL(width, height);

  createFBO();
}

void
Viewer::createFBO()
{
  if (!m_glewInitdone)
    return;
  
  int wd = camera()->screenWidth();
  int ht = camera()->screenHeight();

//  wd/=2;
//  ht/=2;

  if (m_slcBuffer) glDeleteFramebuffers(1, &m_slcBuffer);
  if (m_rboId) glDeleteRenderbuffers(1, &m_rboId);
  if (m_slcTex[0]) glDeleteTextures(2, m_slcTex);  

  glGenFramebuffers(1, &m_slcBuffer);
  glGenRenderbuffers(1, &m_rboId);
  glGenTextures(2, m_slcTex);

  glBindFramebuffer(GL_FRAMEBUFFER, m_slcBuffer);

  glBindRenderbuffer(GL_RENDERBUFFER, m_rboId);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, wd, ht);
  glBindRenderbuffer(GL_RENDERBUFFER, 0);

  // attach the renderbuffer to depth attachment point
  glFramebufferRenderbuffer(GL_FRAMEBUFFER,      // 1. fbo target: GL_FRAMEBUFFER
			    GL_DEPTH_ATTACHMENT, // 2. attachment point
			    GL_RENDERBUFFER,     // 3. rbo target: GL_RENDERBUFFER
			    m_rboId);              // 4. rbo ID

  for(int i=0; i<2; i++)
    {
      glBindTexture(GL_TEXTURE_RECTANGLE_ARB, m_slcTex[i]);
      glTexImage2D(GL_TEXTURE_RECTANGLE_ARB,
		   0,
		   GL_RGBA16,
		   wd, ht,
		   0,
		   GL_RGBA,
		   GL_UNSIGNED_SHORT,
		   0);
    }

  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void
Viewer::createRaycastShader()
{
  QString shaderString;

  int maxSteps = qSqrt(m_vsize.x*m_vsize.x + m_vsize.y*m_vsize.y + m_vsize.z*m_vsize.z);
  shaderString = ShaderFactory::genRaycastShader(maxSteps, !m_fullRender);

  m_rcShader = glCreateProgramObjectARB();
  if (! ShaderFactory::loadShader(m_rcShader,
				  shaderString))
    {
      m_rcShader = 0;
      QMessageBox::information(0, "", "Cannot create rc shader.");
    }

  m_rcParm[0] = glGetUniformLocationARB(m_rcShader, "dataTex");
  m_rcParm[1] = glGetUniformLocationARB(m_rcShader, "lutTex");
  m_rcParm[2] = glGetUniformLocationARB(m_rcShader, "exitTex");
  m_rcParm[3] = glGetUniformLocationARB(m_rcShader, "stepSize");
  m_rcParm[4] = glGetUniformLocationARB(m_rcShader, "eyepos");
  m_rcParm[5] = glGetUniformLocationARB(m_rcShader, "viewDir");
  m_rcParm[6] = glGetUniformLocationARB(m_rcShader, "vcorner");
  m_rcParm[7] = glGetUniformLocationARB(m_rcShader, "vsize");
  m_rcParm[8] = glGetUniformLocationARB(m_rcShader, "minZ");
  m_rcParm[9] = glGetUniformLocationARB(m_rcShader, "maxZ");
  m_rcParm[10]= glGetUniformLocationARB(m_rcShader, "maskTex");
  m_rcParm[11]= glGetUniformLocationARB(m_rcShader, "saveCoord");
  m_rcParm[12]= glGetUniformLocationARB(m_rcShader, "skipLayers");
  m_rcParm[13]= glGetUniformLocationARB(m_rcShader, "tagTex");
}

void
Viewer::createShaders()
{
  QString shaderString;

  createRaycastShader();


  //----------------------
  shaderString = ShaderFactory::genEdgeEnhanceShader();

  m_eeShader = glCreateProgramObjectARB();
  if (! ShaderFactory::loadShader(m_eeShader,
				  shaderString))
    {
      m_eeShader = 0;
      QMessageBox::information(0, "", "Cannot create ee shader.");
    }

  m_eeParm[0] = glGetUniformLocationARB(m_eeShader, "blurTex");
  m_eeParm[1] = glGetUniformLocationARB(m_eeShader, "minZ");
  m_eeParm[2] = glGetUniformLocationARB(m_eeShader, "maxZ");
  m_eeParm[3] = glGetUniformLocationARB(m_eeShader, "eyepos");
  m_eeParm[4] = glGetUniformLocationARB(m_eeShader, "viewDir");
  m_eeParm[5] = glGetUniformLocationARB(m_eeShader, "dzScale");
  m_eeParm[6] = glGetUniformLocationARB(m_eeShader, "tagTex");
  //----------------------


  //----------------------
  shaderString = ShaderFactory::genSliceShader();

  m_sliceShader = glCreateProgramObjectARB();
  if (! ShaderFactory::loadShader(m_sliceShader,
				  shaderString))
    {
      m_sliceShader = 0;
      QMessageBox::information(0, "", "Cannot create slice shader.");
    }

  m_sliceParm[0] = glGetUniformLocationARB(m_sliceShader, "dataTex");
  m_sliceParm[1] = glGetUniformLocationARB(m_sliceShader, "lutTex");
  //----------------------



  //----------------------
  shaderString = ShaderFactory::genRectBlurShaderString(1); // bilateral filter

  m_blurShader = glCreateProgramObjectARB();
  if (! ShaderFactory::loadShader(m_blurShader,
				  shaderString))
    {
      m_blurShader = 0;
      QMessageBox::information(0, "", "Cannot create blur shader.");
    }

  m_blurParm[0] = glGetUniformLocationARB(m_blurShader, "blurTex");
  m_blurParm[1] = glGetUniformLocationARB(m_blurShader, "minZ");
  m_blurParm[2] = glGetUniformLocationARB(m_blurShader, "maxZ");
  //----------------------


  //----------------------
  shaderString = ShaderFactory::genDepthShader();

  m_depthShader = glCreateProgramObjectARB();
  if (! ShaderFactory::loadShader(m_depthShader,
				    shaderString))
    {
      m_depthShader = 0;
      QMessageBox::information(0, "", "Cannot create depth shader.");
    }

  m_depthParm[0] = glGetUniformLocationARB(m_depthShader, "minZ");
  m_depthParm[1] = glGetUniformLocationARB(m_depthShader, "maxZ");
  m_depthParm[2] = glGetUniformLocationARB(m_depthShader, "eyepos");
  m_depthParm[3] = glGetUniformLocationARB(m_depthShader, "viewDir");
  //----------------------


  //----------------------
  shaderString = ShaderFactory::genFinalPointShader();

  m_finalPointShader = glCreateProgramObjectARB();
  if (! ShaderFactory::loadShader(m_finalPointShader,
				    shaderString))
    {
      m_finalPointShader = 0;
      QMessageBox::information(0, "", "Cannot create final point shader.");
    }

  m_fpsParm[0] = glGetUniformLocationARB(m_finalPointShader, "blurTex");
  m_fpsParm[1] = glGetUniformLocationARB(m_finalPointShader, "minZ");
  m_fpsParm[2] = glGetUniformLocationARB(m_finalPointShader, "maxZ");
  m_fpsParm[3] = glGetUniformLocationARB(m_finalPointShader, "eyepos");
  m_fpsParm[4] = glGetUniformLocationARB(m_finalPointShader, "viewDir");
  m_fpsParm[5] = glGetUniformLocationARB(m_finalPointShader, "dzScale");
  
}


void Viewer::setShowSlices(bool b) { m_showSlices = b; }

void
Viewer::updateSlices()
{
  setDSlice(m_dslice);
  setWSlice(m_wslice);
  setHSlice(m_hslice);
}


void Viewer::setVoxelChoice(int p) { m_voxChoice = p; }
void Viewer::setVoxelInterval(int p)
{
  m_pointSkip = p;
  setDSlice(m_dslice);
  setWSlice(m_wslice);
  setHSlice(m_hslice);
}

void
Viewer::updateCurrSlice(int cst, int cs)
{
  m_currSliceType = cst;
  m_currSlice = cs;
  update();
}

void
Viewer::setPaintedTags(QList<int> t)
{
  m_paintedTags = t;
  update();
}

void
Viewer::setCurveTags(QList<int> t)
{
  m_curveTags = t;
  update();
}

void
Viewer::setFiberTags(QList<int> t)
{
  m_fiberTags = t;
  update();
}


void Viewer::setMaskDataPtr(uchar *ptr) { m_maskPtr = ptr; }
void Viewer::setVolDataPtr(uchar *ptr) { m_volPtr = ptr; }

void
Viewer::updateViewerBox(int minD, int maxD, int minW, int maxW, int minH, int maxH)
{
  m_minDSlice = minD;
  m_maxDSlice = maxD;

  m_minWSlice = minW;
  m_maxWSlice = maxW;

  m_minHSlice = minH;
  m_maxHSlice = maxH;

  setSceneCenter(Vec((m_maxHSlice+m_minHSlice),
		     (m_maxWSlice+m_minWSlice),
		     (m_maxDSlice+m_minDSlice))/2);		 

  
  m_clipPlanes->setBounds(Vec(m_minHSlice,
			      m_minWSlice,
			      m_minDSlice),
			  Vec(m_maxHSlice,
			      m_maxWSlice,
			      m_maxDSlice));

  m_boundingBox.setPositions(Vec(m_minHSlice,
				 m_minWSlice,
				 m_minDSlice),
			     Vec(m_maxHSlice,
				 m_maxWSlice,
				 m_maxDSlice));
}

void
Viewer::setShowBox(bool b)
{
  m_showBox = b;
  if (m_showBox)
    m_boundingBox.activateBounds();
  else
    m_boundingBox.deactivateBounds();

  update();  
}

void
Viewer::keyPressEvent(QKeyEvent *event)
{
  if (event->key() == Qt::Key_Escape)
    return;

  if (m_boundingBox.keyPressEvent(event))
    return;
  
  
  if (event->key() == Qt::Key_A)
    {  
      toggleAxisIsDrawn();
      update();
      return;
    }

  if (event->key() == Qt::Key_F)
    {  
      m_fullRender = !m_fullRender;
      createRaycastShader();
      update();
      return;
    }

  if (event->key() == Qt::Key_R)
    {  
      // reset bitmap
      m_bitmask.fill(true);
      update();
      return;
    }


  // process clipplane events
  if (m_clipPlanes->keyPressEvent(event))
    {
      update();
      return;
    }

  if (event->key() == Qt::Key_P)
    {
      camera()->setType(Camera::PERSPECTIVE);
      update();
      return;
    }

  if (event->key() == Qt::Key_O)
    {
      camera()->setType(Camera::ORTHOGRAPHIC);
      update();
      return;
    }

  if (event->key() == Qt::Key_V)
    {
      if (m_clipPlanes->count() > 0)
	{
	  bool show = m_clipPlanes->show(0);
	  if (show)
	    m_clipPlanes->hide();
	  else
	    m_clipPlanes->show();
	}
      return;
    }


  if (event->key() == Qt::Key_Plus ||
      event->key() == Qt::Key_Minus)
    {
      if (event->key() == Qt::Key_Plus)
	m_skipLayers++;

      if (event->key() == Qt::Key_Minus)
	m_skipLayers = qMax(m_skipLayers-1, 0);

      update();
      return;
    }


  if (event->key() != Qt::Key_H)
    QGLViewer::keyPressEvent(event);
}

void
Viewer::setGridSize(int d, int w, int h)
{
  m_depth = d;
  m_width = w;
  m_height = h;

  qint64 bsz = m_depth*m_width*m_height;
  m_bitmask.resize(bsz);
  m_bitmask.fill(true);

  m_minDSlice = 0;
  m_minWSlice = 0;
  m_minHSlice = 0;

  m_maxDSlice = d-1;
  m_maxWSlice = w-1;
  m_maxHSlice = h-1;

  m_clipPlanes->setBounds(Vec(m_minHSlice,
			      m_minWSlice,
			      m_minDSlice),
			  Vec(m_maxHSlice,
			      m_maxWSlice,
			      m_maxDSlice));
  
  m_boundingBox.setBounds(Vec(m_minHSlice,
			      m_minWSlice,
			      m_minDSlice),
			  Vec(m_maxHSlice,
			      m_maxWSlice,
			      m_maxDSlice));


  setSceneBoundingBox(Vec(0,0,0), Vec(m_height, m_width, m_depth));
  setSceneCenter(Vec((m_maxHSlice+m_minHSlice),
		     (m_maxWSlice+m_minWSlice),
		     (m_maxDSlice+m_minDSlice))/2);		 
  showEntireScene();
}

void
Viewer::drawEnclosingCube(Vec subvolmin,
			  Vec subvolmax)
{
  glBegin(GL_QUADS);  
  glVertex3f(subvolmin.x, subvolmin.y, subvolmin.z);
  glVertex3f(subvolmax.x, subvolmin.y, subvolmin.z);
  glVertex3f(subvolmax.x, subvolmax.y, subvolmin.z);
  glVertex3f(subvolmin.x, subvolmax.y, subvolmin.z);
  glEnd();
  
  // FRONT 
  glBegin(GL_QUADS);  
  glVertex3f(subvolmin.x, subvolmin.y, subvolmax.z);
  glVertex3f(subvolmax.x, subvolmin.y, subvolmax.z);
  glVertex3f(subvolmax.x, subvolmax.y, subvolmax.z);
  glVertex3f(subvolmin.x, subvolmax.y, subvolmax.z);
  glEnd();
  
  // TOP
  glBegin(GL_QUADS);  
  glVertex3f(subvolmin.x, subvolmax.y, subvolmin.z);
  glVertex3f(subvolmax.x, subvolmax.y, subvolmin.z);
  glVertex3f(subvolmax.x, subvolmax.y, subvolmax.z);
  glVertex3f(subvolmin.x, subvolmax.y, subvolmax.z);
  glEnd();
  
  // BOTTOM
  glBegin(GL_QUADS);  
  glVertex3f(subvolmin.x, subvolmin.y, subvolmin.z);
  glVertex3f(subvolmax.x, subvolmin.y, subvolmin.z);
  glVertex3f(subvolmax.x, subvolmin.y, subvolmax.z);
  glVertex3f(subvolmin.x, subvolmin.y, subvolmax.z);  
  glEnd();    
}

void
Viewer::drawCurrentSlice(Vec subvolmin,
			 Vec subvolmax)
{
  if (m_currSliceType == 0)
    {
      glBegin(GL_QUADS);  
      glVertex3f(subvolmin.x, subvolmin.y, m_currSlice);
      glVertex3f(subvolmax.x, subvolmin.y, m_currSlice);
      glVertex3f(subvolmax.x, subvolmax.y, m_currSlice);
      glVertex3f(subvolmin.x, subvolmax.y, m_currSlice);
      glEnd();  
    }

  if (m_currSliceType == 1)
    {
      glBegin(GL_QUADS);  
      glVertex3f(subvolmin.x, m_currSlice, subvolmin.z);
      glVertex3f(subvolmax.x, m_currSlice, subvolmin.z);
      glVertex3f(subvolmax.x, m_currSlice, subvolmax.z);
      glVertex3f(subvolmin.x, m_currSlice, subvolmax.z);  
      glEnd();    
    }

  if (m_currSliceType == 2)
    {
      glBegin(GL_QUADS);  
      glVertex3f(m_currSlice, subvolmin.y, subvolmin.z);
      glVertex3f(m_currSlice, subvolmax.y, subvolmin.z);
      glVertex3f(m_currSlice, subvolmax.y, subvolmax.z);
      glVertex3f(m_currSlice, subvolmin.y, subvolmax.z);  
      glEnd();    
    }
}

void
Viewer::drawBox()
{

  //setAxisIsDrawn();
  
  glColor3d(0.5,0.5,0.5);

  glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
  
  glLineWidth(1);
  drawEnclosingCube(Vec(0,0,0),
		    Vec(m_height, m_width, m_depth));
  
  glLineWidth(2);
  glColor3d(0.8,0.8,0.8);
  drawEnclosingCube(Vec(m_minHSlice, m_minWSlice, m_minDSlice),
		    Vec(m_maxHSlice, m_maxWSlice, m_maxDSlice));
  
  
  glLineWidth(3);
  glColor3d(1.0,0.85,0.7);
  drawCurrentSlice(Vec(0,0,0),
		   Vec(m_height, m_width, m_depth));
  
  glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

  glLineWidth(1);
}

void
Viewer::setMultiMapCurves(int type, QMultiMap<int, Curve*> *cg)
{
  if (type == 0) m_Dcg = cg;
  if (type == 1) m_Wcg = cg;
  if (type == 2) m_Hcg = cg;
}

void
Viewer::setListMapCurves(int type, QList< QMap<int, Curve> > *cg)
{
  if (type == 0) m_Dmcg = cg;
  if (type == 1) m_Wmcg = cg;
  if (type == 2) m_Hmcg = cg;
}

void
Viewer::setShrinkwrapCurves(int type, QList< QMultiMap<int, Curve*> > *cg)
{
  if (type == 0) m_Dswcg = cg;
  if (type == 1) m_Wswcg = cg;
  if (type == 2) m_Hswcg = cg;
}

void
Viewer::setFibers(QList<Fiber*> *fb)
{
  m_fibers = fb;
}

void
Viewer::drawPointsWithoutShader()
{
  if (!m_volPtr || !m_maskPtr)
    return;

  glEnable(GL_ALPHA_TEST);
  glAlphaFunc(GL_GREATER, 0.1);

  //--------------------------------
  // set point scaling based on distance to the viewer

  GLfloat sizes[2];
  GLfloat coeff[] = {1.0, 0.0, 0.0}; // constant, linear, quadratic
  // ptsize = PointSize*sqrt(1/(constant + linear*d + quadratic*d*d))

  glGetFloatv(GL_ALIASED_POINT_SIZE_RANGE, sizes);
  glPointParameterf(GL_POINT_SIZE_MAX, sizes[1]);
  glPointParameterf(GL_POINT_SIZE_MIN, sizes[0]);
  glPointParameterfv(GL_POINT_DISTANCE_ATTENUATION, coeff);

  Vec box[8];
  box[0] = Vec(m_minHSlice, m_minWSlice, m_minDSlice);
  box[1] = Vec(m_minHSlice, m_minWSlice, m_maxDSlice);
  box[2] = Vec(m_minHSlice, m_maxWSlice, m_maxDSlice);
  box[3] = Vec(m_minHSlice, m_maxWSlice, m_minDSlice);
  box[4] = Vec(m_maxHSlice, m_minWSlice, m_minDSlice);
  box[5] = Vec(m_maxHSlice, m_minWSlice, m_maxDSlice);
  box[6] = Vec(m_maxHSlice, m_maxWSlice, m_maxDSlice);
  box[7] = Vec(m_maxHSlice, m_maxWSlice, m_minDSlice);
  float pglmax = 0;
  float pglmin = 100000;
  for(int b=0; b<8; b++)
    {
      float cpgl = camera()->pixelGLRatio(box[b]);
      pglmin = qMin(pglmin, cpgl);
      pglmax = qMax(pglmax, cpgl);
    }
  float pglr = (pglmax+pglmin)*0.5;
  int ptsz = m_pointScaling*m_pointSize/pglr;
  glPointSize(ptsz);
  //--------------------------------

  if (m_pointSkip > 0 && m_maskPtr)
    drawVolMask();
}

void
Viewer::draw()
{
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glEnable(GL_DEPTH_TEST);

  glDisable(GL_LIGHTING);

  if ((!m_volPtr || !m_maskPtr) && m_showBox)
    {
      m_boundingBox.draw();
      drawBox();
    }

  drawMMDCurve();
  drawMMWCurve();
  drawMMHCurve();

  drawLMDCurve();
  drawLMWCurve();
  drawLMHCurve();

  drawSWDCurve();
  drawSWWCurve();
  drawSWHCurve();

  glEnable(GL_LIGHTING);
  drawFibers();
  glDisable(GL_LIGHTING);


  if (!m_volPtr || !m_maskPtr)
    return;

//  glEnable(GL_ALPHA_TEST);
//  glAlphaFunc(GL_GREATER, 0.1);

  GLfloat sizes[2];
  GLfloat coeff[] = {1.0, 0.0, 0.0}; // constant, linear, quadratic
  // ptsize = PointSize*sqrt(1/(constant + linear*d + quadratic*d*d))

  glGetFloatv(GL_ALIASED_POINT_SIZE_RANGE, sizes);
  glPointParameterf(GL_POINT_SIZE_MAX, sizes[1]);
  glPointParameterf(GL_POINT_SIZE_MIN, sizes[0]);
  glPointParameterfv(GL_POINT_DISTANCE_ATTENUATION, coeff);

  Vec box[8];
  box[0] = Vec(m_minHSlice, m_minWSlice, m_minDSlice);
  box[1] = Vec(m_minHSlice, m_minWSlice, m_maxDSlice);
  box[2] = Vec(m_minHSlice, m_maxWSlice, m_maxDSlice);
  box[3] = Vec(m_minHSlice, m_maxWSlice, m_minDSlice);
  box[4] = Vec(m_maxHSlice, m_minWSlice, m_minDSlice);
  box[5] = Vec(m_maxHSlice, m_minWSlice, m_maxDSlice);
  box[6] = Vec(m_maxHSlice, m_maxWSlice, m_maxDSlice);
  box[7] = Vec(m_maxHSlice, m_maxWSlice, m_minDSlice);

  //--------------------------------
  Vec eyepos = camera()->position();
  Vec viewDir = camera()->viewDirection();
  float minZ = 1000000;
  float maxZ = -1000000;
  for(int b=0; b<8; b++)
    {
      float zv = (box[b]-eyepos)*viewDir;
      minZ = qMin(minZ, zv);
      maxZ = qMax(maxZ, zv);
    }
  //--------------------------------

  //--------------------------------
  // set point scaling based on distance to the viewer
  float pglmax = 0;
  float pglmin = 100000;
  for(int b=0; b<8; b++)
    {
      float cpgl = camera()->pixelGLRatio(box[b]);
      pglmin = qMin(pglmin, cpgl);
      pglmax = qMax(pglmax, cpgl);
    }
  float pglr = (pglmax+pglmin)*0.5;
  int ptsz = m_pointScaling*m_pointSize/pglr;
  glPointSize(ptsz);
  //--------------------------------



//  glBindFramebuffer(GL_FRAMEBUFFER_EXT, m_slcBuffer);
//  for(int fbn=0; fbn<2; fbn++)
//    {
//      glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER,
//			     GL_COLOR_ATTACHMENT0_EXT,
//			     GL_TEXTURE_RECTANGLE_ARB,
//			     m_slcTex[fbn],
//			     0);
//      glDrawBuffer(GL_COLOR_ATTACHMENT0_EXT);
//      glClearColor(0, 0, 0, 0);
//      glClear(GL_COLOR_BUFFER_BIT);
//      glClear(GL_DEPTH_BUFFER_BIT);
//    }
//  glBindFramebuffer(GL_FRAMEBUFFER_EXT, 0);
//
//  //--------------------------------
//  glBindFramebuffer(GL_FRAMEBUFFER_EXT, m_slcBuffer);
//  glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER,
//			 GL_COLOR_ATTACHMENT0_EXT,
//			 GL_TEXTURE_RECTANGLE_ARB,
//			 m_slcTex[0],
//			 0);
//  glDrawBuffer(GL_COLOR_ATTACHMENT0_EXT);  
//
//  glUseProgramObjectARB(m_depthShader);
//  glUniform1fARB(m_depthParm[0], minZ); // minZ
//  glUniform1fARB(m_depthParm[1], maxZ); // maxZ
//  glUniform3fARB(m_depthParm[2], eyepos.x, eyepos.y, eyepos.z); // eyepos
//  glUniform3fARB(m_depthParm[3], viewDir.x, viewDir.y, viewDir.z); // viewDir
//  //--------------------------------
//
//  if (m_pointSkip > 0 && m_maskPtr)
//    drawVolMask();
//
//  glBindFramebuffer(GL_FRAMEBUFFER_EXT, 0);
//  //--------------------------------
//
//
//  glBindFramebuffer(GL_FRAMEBUFFER_EXT, m_slcBuffer);
//  glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER,
//			 GL_COLOR_ATTACHMENT0_EXT,
//			 GL_TEXTURE_RECTANGLE_ARB,
//			 m_slcTex[1],
//			 0);
//  glDrawBuffer(GL_COLOR_ATTACHMENT0_EXT);  
//
//  glUseProgramObjectARB(m_blurShader);
//  glActiveTexture(GL_TEXTURE1);
//  glEnable(GL_TEXTURE_RECTANGLE_ARB);
//  glBindTexture(GL_TEXTURE_RECTANGLE_ARB, m_slcTex[0]);
//  glUniform1iARB(m_blurParm[0], 1); // blurTex
//  glUniform1fARB(m_blurParm[1], minZ); // minZ
//  glUniform1fARB(m_blurParm[2], maxZ); // maxZ
//
//  int wd = camera()->screenWidth();
//  int ht = camera()->screenHeight();
//  StaticFunctions::pushOrthoView(0, 0, wd, ht);
//  StaticFunctions::drawQuad(0, 0, wd, ht, 1);
//  StaticFunctions::popOrthoView();
//
//  glBindFramebuffer(GL_FRAMEBUFFER_EXT, 0);
//  //--------------------------------
//
//
//
//  //--------------------------------
//  glUseProgramObjectARB(m_finalPointShader);
//  glActiveTexture(GL_TEXTURE1);
//  glEnable(GL_TEXTURE_RECTANGLE_ARB);
//  glBindTexture(GL_TEXTURE_RECTANGLE_ARB, m_slcTex[1]);
//  glUniform1iARB(m_fpsParm[0], 1); // slctex
//  glUniform1fARB(m_fpsParm[1], minZ); // minZ
//  glUniform1fARB(m_fpsParm[2], maxZ); // maxZ
//  glUniform3fARB(m_fpsParm[3], eyepos.x, eyepos.y, eyepos.z); // eyepos
//  glUniform3fARB(m_fpsParm[4], viewDir.x, viewDir.y, viewDir.z); // viewDir
//  glUniform1fARB(m_fpsParm[5], m_dzScale); // dzScale
//
//  glPointSize(ptsz);
//
//  if (m_pointSkip > 0 && m_maskPtr)
//    drawVolMask();
//
//  //--------------------------------
//  glUseProgramObjectARB(0);
//  //--------------------------------
//
//  glActiveTexture(GL_TEXTURE1);
//  glDisable(GL_TEXTURE_RECTANGLE_ARB);
//
//
//  if (m_showSlices)
//    drawSlices();

//  drawClip();

  volumeRaycast(minZ, maxZ, false); // run full raycast process
  
//  drawClipSlices();


  glDisable(GL_DEPTH_TEST);
  glEnable(GL_BLEND);
  glBlendFunc(GL_ONE, GL_ONE);

  m_clipPlanes->draw(this, false);
  
  if (m_showBox)
    {
      m_boundingBox.draw();
      drawBox();
    }

  glEnable(GL_DEPTH_TEST);

  drawInfo();
}

void
Viewer::drawInfo()
{
  glDisable(GL_DEPTH_TEST);

  QFont tfont = QFont("Helvetica", 12);  
  QString mesg;

  mesg += QString("LOD(%1) Vol(%2 %3 %4) ").\
    arg(m_sslevel).arg(m_vsize.x).arg(m_vsize.y).arg(m_vsize.z);

  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glEnable(GL_POINT_SMOOTH);
      
  if (m_paintHit || m_carveHit)
    {
      float cpgl = camera()->pixelGLRatio(m_target);
      int d = m_target.z;
      int w = m_target.y;
      int h = m_target.x;
      int tag = Global::tag();
      float r = Global::tagColors()[4*tag+0]*1.0/255.0;
      float g = Global::tagColors()[4*tag+1]*1.0/255.0;
      float b = Global::tagColors()[4*tag+2]*1.0/255.0;

      glEnable(GL_POINT_SPRITE_ARB);
      glEnable(GL_TEXTURE_2D);
      glBindTexture(GL_TEXTURE_2D, Global::hollowSpriteTexture());
      glTexEnvi(GL_POINT_SPRITE, GL_COORD_REPLACE, GL_TRUE);
      glPointSize(3*Global::spread()/cpgl);
      glBegin(GL_POINTS);
      glColor4f(r*0.5, g*0.5, b*0.5, 0.5);
      glVertex3f(h,w,d);
      glEnd();

      glDisable(GL_POINT_SPRITE);
      glDisable(GL_TEXTURE_2D);

      mesg += QString("%1 %2 %3").arg(h).arg(w).arg(d);
    }
  
  int wd = camera()->screenWidth();
  int ht = camera()->screenHeight();
  StaticFunctions::pushOrthoView(0, 0, wd, ht);
  StaticFunctions::renderText(10,10, mesg, tfont, Qt::black, Qt::lightGray);
  StaticFunctions::popOrthoView();

  glEnable(GL_DEPTH_TEST);
  glDisable(GL_BLEND);
}

void
Viewer::drawFibers()
{
  if (!m_fibers) return;

  glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

  bool noneselected = true;
  for(int i=0; i<m_fibers->count(); i++)
    {
      Fiber *fb = m_fibers->at(i);
      if (fb->selected)
	{
	  noneselected = false;
	  break;
	}
    }
 
  for(int i=0; i<m_fibers->count(); i++)
    {
      Fiber *fb = m_fibers->at(i);
      int tag = fb->tag;
      if (m_fiberTags.count() == 0 ||
	  m_fiberTags[0] == -1 ||
	  m_fiberTags.contains(tag))
	{
	  float r = Global::tagColors()[4*tag+0]*1.0/255.0;
	  float g = Global::tagColors()[4*tag+1]*1.0/255.0;
	  float b = Global::tagColors()[4*tag+2]*1.0/255.0;
	  glColor3f(r,g,b);

	  if (noneselected ||
	      (!noneselected && fb->selected))
	    {
	      glEnable(GL_LIGHTING);
	      QList<Vec> tube = fb->tube();
	      glBegin(GL_TRIANGLE_STRIP);
	      for(int t=0; t<tube.count()/2; t++)
		{
		  glNormal3fv(tube[2*t+0]);	      
		  glVertex3fv(tube[2*t+1]);
		}
	      glEnd();
	    }
	  else
	    {
	      glDisable(GL_LIGHTING);
	      glLineWidth(1);
	      glBegin(GL_LINE_STRIP);
	      for(int j=0; j<fb->smoothSeeds.count(); j++)
		glVertex3fv(fb->smoothSeeds[j]);
	      glEnd();
	    }
	}
    }

  glLineWidth(1);
}

bool
Viewer::clip(int d, int w, int h)
{
  QList<Vec> cPos =  m_clipPlanes->positions();
  QList<Vec> cNorm = m_clipPlanes->normals();

  for(int i=0; i<cPos.count(); i++)
    {
      Vec cpos = cPos[i];
      Vec cnorm = cNorm[i];
      
      Vec p = Vec(h, w, d) - cpos;
      if (cnorm*p > 0)
	return true;
    }

  return false;
}

void
Viewer::updateClipVoxels()
{
  m_clipVoxels.clear();

  if (!m_volPtr || !m_maskPtr)
    return;
  
  QList<Vec> cPos =  m_clipPlanes->positions();
  QList<Vec> cNorm = m_clipPlanes->normals();

  uchar *lut = Global::lut();

  Vec box[8];
  box[0] = Vec(m_minHSlice, m_minWSlice, m_minDSlice);
  box[1] = Vec(m_minHSlice, m_minWSlice, m_maxDSlice);
  box[2] = Vec(m_minHSlice, m_maxWSlice, m_maxDSlice);
  box[3] = Vec(m_minHSlice, m_maxWSlice, m_minDSlice);
  box[4] = Vec(m_maxHSlice, m_minWSlice, m_minDSlice);
  box[5] = Vec(m_maxHSlice, m_minWSlice, m_maxDSlice);
  box[6] = Vec(m_maxHSlice, m_maxWSlice, m_maxDSlice);
  box[7] = Vec(m_maxHSlice, m_maxWSlice, m_minDSlice);

  for(int i=0; i<cPos.count(); i++)
    {
      if (m_clipPlanes->show(i))
	{
	  Vec cpos = cPos[i];
	  Vec cnorm = cNorm[i];

	  Vec xaxis = cnorm.orthogonalVec().unit();
	  Vec yaxis = cnorm ^ xaxis;

	  //--- drop perpendiculars onto normal from all 8 vertices of the subvolume 
	  float boxdist[8];
	  
	  // get width
	  for (int i=0; i<8; i++) boxdist[i] = (box[i] - cpos)*xaxis;
	  float wmin = boxdist[0];
	  for (int i=1; i<8; i++) wmin = qMin(wmin, boxdist[i]);
	  float wmax = boxdist[0];
	  for (int i=1; i<8; i++) wmax = qMax(wmax, boxdist[i]);
	  //------------------------
	  
	  // get height
	  for (int i=0; i<8; i++) boxdist[i] = (box[i] - cpos)*yaxis;
	  float hmin = boxdist[0];
	  for (int i=1; i<8; i++) hmin = qMin(hmin, boxdist[i]);
	  float hmax = boxdist[0];
	  for (int i=1; i<8; i++) hmax = qMax(hmax, boxdist[i]);
	  //------------------------
	  
	  
	  for(int a=(int)wmin; a<(int)wmax; a++)
	    for(int b=(int)hmin; b<(int)hmax; b++)
	      {
		Vec co = cpos+a*xaxis+b*yaxis;	
		int d = co.z;
		int w = co.y;
		int h = co.x;
		if (d > m_minDSlice && d < m_maxDSlice &&
		    w > m_minWSlice && w < m_maxWSlice &&
		    h > m_minHSlice && h < m_maxHSlice)
		  {
		    uchar vol = m_volPtr[d*m_width*m_height + w*m_height + h];
		    if (lut[4*vol+3] > 10)
		      m_clipVoxels << d << w << h << vol;
		  }
	      }
	}
    }
  //-------------------------------------
}


void
Viewer::updateVoxels()
{
  m_voxels.clear();
  
  if (!m_volPtr || !m_maskPtr || m_pointSkip == 0)
    {
      if (!m_volPtr || !m_maskPtr)
	QMessageBox::information(0, "", "Data not loaded into memory, therefore cannot show the voxels");
      else if (m_pointSkip == 0)
	QMessageBox::information(0, "", "Step size is set to 0, therefore will not show the voxels");
      return;
    }

  Vec bmin, bmax;
  m_boundingBox.bounds(bmin, bmax);
  emit updateSliceBounds(bmin, bmax);

  setSceneCenter((bmax+bmin)/2);

  m_minDSlice = bmin.z;
  m_minWSlice = bmin.y;
  m_minHSlice = bmin.x;
  m_maxDSlice = bmax.z;
  m_maxWSlice = bmax.y;
  m_maxHSlice = bmax.x;


  if (m_voxChoice == 0)
    {
      updateVoxelsWithTF();
      return;
    }
  
  QProgressDialog progress("Updating voxel structure",
			   QString(),
			   0, 100,
			   0);
  progress.setMinimumDuration(0);

  bool takeall = (m_paintedTags.count() == 0 ||
		  m_paintedTags[0] == -1);

  //----------------------------------
  // get the edges first  
  int d,w,h;
  d=m_minDSlice;
  for(int w=m_minWSlice; w<m_maxWSlice; w+=m_pointSkip)
    for(int h=m_minHSlice; h<m_maxHSlice; h+=m_pointSkip)
      {
	if (!clip(d, w, h))
	  {
	    uchar tag = m_maskPtr[d*m_width*m_height + w*m_height + h];
	    if (m_paintedTags.contains(tag) ||
		(tag > 0 && takeall))
	      {
		uchar vol = m_volPtr[d*m_width*m_height + w*m_height + h];
		m_voxels << d << w << h << tag << vol;
	      }
	  }
      }
  w=m_minWSlice;
  for(int d=m_minDSlice; d<m_maxDSlice; d+=m_pointSkip)
    for(int h=m_minHSlice; h<m_maxHSlice; h+=m_pointSkip)
      {
	if (!clip(d, w, h))
	  {
	    uchar tag = m_maskPtr[d*m_width*m_height + w*m_height + h];
	    if (m_paintedTags.contains(tag) ||
		(tag > 0 && takeall))
	      {
		uchar vol = m_volPtr[d*m_width*m_height + w*m_height + h];
		m_voxels << d << w << h << tag << vol;
	      }
	  }
      }
  h=m_minHSlice;
  for(int d=m_minDSlice; d<m_maxDSlice; d+=m_pointSkip)
    for(int w=m_minWSlice; w<m_maxWSlice; w+=m_pointSkip)
      {
	if (!clip(d, w, h))
	  {
	    uchar tag = m_maskPtr[d*m_width*m_height + w*m_height + h];
	    if (m_paintedTags.contains(tag) ||
		(tag > 0 && takeall))
	      {
		uchar vol = m_volPtr[d*m_width*m_height + w*m_height + h];
		m_voxels << d << w << h << tag << vol;
	      }
	  }
      }
  d=m_maxDSlice;
  for(int w=m_minWSlice; w<m_maxWSlice; w+=m_pointSkip)
    for(int h=m_minHSlice; h<m_maxHSlice; h+=m_pointSkip)
      {
	if (!clip(d, w, h))
	  {
	    uchar tag = m_maskPtr[d*m_width*m_height + w*m_height + h];
	    if (m_paintedTags.contains(tag) ||
		(tag > 0 && takeall))
	      {
		uchar vol = m_volPtr[d*m_width*m_height + w*m_height + h];
		m_voxels << d << w << h << tag << vol;
	      }
	  }
      }
  w=m_maxWSlice;
  for(int d=m_minDSlice; d<m_maxDSlice; d+=m_pointSkip)
    for(int h=m_minHSlice; h<m_maxHSlice; h+=m_pointSkip)
      {
	if (!clip(d, w, h))
	  {
	    uchar tag = m_maskPtr[d*m_width*m_height + w*m_height + h];
	    if (m_paintedTags.contains(tag) ||
		(tag > 0 && takeall))
	      {
		uchar vol = m_volPtr[d*m_width*m_height + w*m_height + h];
		m_voxels << d << w << h << tag << vol;
	      }
	  }
      }
  h=m_maxHSlice;
  for(int d=m_minDSlice; d<m_maxDSlice; d+=m_pointSkip)
    for(int w=m_minWSlice; w<m_maxWSlice; w+=m_pointSkip)
      {
	if (!clip(d, w, h))
	  {
	    uchar tag = m_maskPtr[d*m_width*m_height + w*m_height + h];
	    if (m_paintedTags.contains(tag) ||
		(tag > 0 && takeall))
	      {
		uchar vol = m_volPtr[d*m_width*m_height + w*m_height + h];
		m_voxels << d << w << h << tag << vol;
	      }
	  }
      }
  //----------------------------------


  for(d=m_minDSlice+1; d<m_maxDSlice-1; d+=m_pointSkip)
    {
      progress.setValue(100*(float)(d-m_minDSlice)/(m_maxDSlice-m_minDSlice));
      qApp->processEvents();
      for(w=m_minWSlice+1; w<m_maxWSlice-1; w+=m_pointSkip)
	{
	  for(h=m_minHSlice+1; h<m_maxHSlice-1; h+=m_pointSkip)
	    {
	      if (!clip(d, w, h))
		{
		  uchar tag = m_maskPtr[d*m_width*m_height + w*m_height + h];
		  if (m_paintedTags.contains(tag) ||
		      (tag > 0 && takeall))
		    {
		      bool ok = false;
		      for(int dd=-m_pointSkip; dd<=m_pointSkip; dd++)
			for(int ww=-m_pointSkip; ww<=m_pointSkip; ww++)
			  for(int hh=-m_pointSkip; hh<=m_pointSkip; hh++)
			    {
			      int d1 = qBound(m_minDSlice, d+dd, m_maxDSlice);
			      int w1 = qBound(m_minWSlice, w+ww, m_maxWSlice);
			      int h1 = qBound(m_minHSlice, h+hh, m_maxHSlice);
			      if (m_maskPtr[d1*m_width*m_height + w1*m_height + h1] != tag)
				{
				  ok = true;
				  break;
				}
			    }
		      
		      if (ok)
			{
			  uchar vol = m_volPtr[d*m_width*m_height + w*m_height + h];
			  m_voxels << d << w << h << tag << vol;
			}
		    }
		}
	    }
	}
    }

  //----------------------

  //----------------------


  progress.setValue(100);

  update();  
}

void
Viewer::drawVolMask()
{
  if (!m_volPtr || !m_maskPtr || m_pointSkip == 0)
    return;

  if (m_voxChoice == 0)
    {
      drawVol();
      return;
    }

  Vec bmin, bmax;
  m_boundingBox.bounds(bmin, bmax);

  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glEnable(GL_POINT_SMOOTH);

  glEnable(GL_POINT_SPRITE_ARB);
  glEnable(GL_TEXTURE_2D);
  glBindTexture(GL_TEXTURE_2D, Global::spriteTexture());
  glTexEnvi(GL_POINT_SPRITE, GL_COORD_REPLACE, GL_TRUE);

  glBegin(GL_POINTS);
  int nv = m_voxels.count()/5;
  for(int i=0; i<nv; i++)
    {
      int d = m_voxels[5*i+0];
      int w = m_voxels[5*i+1];
      int h = m_voxels[5*i+2];

      if (d < bmin.z || d > bmax.z ||
	  w < bmin.y || w > bmax.y ||
	  h < bmin.x || h > bmax.x)
	{}
      else
	{
	  int t = m_voxels[5*i+3];
	  float v = (float)m_voxels[5*i+4]/255.0;

	  float r = Global::tagColors()[4*t+0]*1.0/255.0;
	  float g = Global::tagColors()[4*t+1]*1.0/255.0;
	  float b = Global::tagColors()[4*t+2]*1.0/255.0;
	  r = r*0.3 + 0.7*v;
	  g = g*0.3 + 0.7*v;
	  b = b*0.3 + 0.7*v;
	  glColor3f(r,g,b);
	  glVertex3f(h, w, d);
	}
    }
  glEnd();

  glDisable(GL_POINT_SPRITE);
  glDisable(GL_TEXTURE_2D);

  glDisable(GL_POINT_SMOOTH);
  glBlendFunc(GL_NONE, GL_NONE);
  glDisable(GL_BLEND);
}

void
Viewer::updateVoxelsWithTF()
{
  m_voxels.clear();
  
  if (!m_volPtr || !m_maskPtr || m_pointSkip == 0)
    return;

  uchar *lut = Global::lut();

  QProgressDialog progress("Updating voxel structure",
			   QString(),
			   0, 100,
			   0);
  progress.setMinimumDuration(0);

//  //----------------------------------
//  // get the edges first
//  int d,w,h;
//  d=m_minDSlice;
//  for(int w=m_minWSlice; w<m_maxWSlice; w+=m_pointSkip)
//    for(int h=m_minHSlice; h<m_maxHSlice; h+=m_pointSkip)
//      {
//	if (!clip(d, w, h))
//	  {
//	    uchar vol = m_volPtr[d*m_width*m_height + w*m_height + h];
//	    if (lut[4*vol+3] > 10)
//	      m_voxels << d << w << h << vol;
//	  }
//      }
//  w=m_minWSlice;
//  for(int d=m_minDSlice; d<m_maxDSlice; d+=m_pointSkip)
//    for(int h=m_minHSlice; h<m_maxHSlice; h+=m_pointSkip)
//      {
//	if (!clip(d, w, h))
//	  {
//	    uchar vol = m_volPtr[d*m_width*m_height + w*m_height + h];
//	    if (lut[4*vol+3] > 10)
//	      m_voxels << d << w << h << vol;
//	  }
//      }
//  h=m_minHSlice;
//  for(int d=m_minDSlice; d<m_maxDSlice; d+=m_pointSkip)
//    for(int w=m_minWSlice; w<m_maxWSlice; w+=m_pointSkip)
//      {
//	if (!clip(d, w, h))
//	  {
//	    uchar vol = m_volPtr[d*m_width*m_height + w*m_height + h];
//	    if (lut[4*vol+3] > 10)
//	      m_voxels << d << w << h << vol;
//	  }
//      }
//  d=m_maxDSlice;
//  for(int w=m_minWSlice; w<m_maxWSlice; w+=m_pointSkip)
//    for(int h=m_minHSlice; h<m_maxHSlice; h+=m_pointSkip)
//      {
//	if (!clip(d, w, h))
//	  {
//	    uchar vol = m_volPtr[d*m_width*m_height + w*m_height + h];
//	    if (lut[4*vol+3] > 10)
//	      m_voxels << d << w << h << vol;
//	  }
//      }
//  w=m_maxWSlice;
//  for(int d=m_minDSlice; d<m_maxDSlice; d+=m_pointSkip)
//    for(int h=m_minHSlice; h<m_maxHSlice; h+=m_pointSkip)
//      {
//	if (!clip(d, w, h))
//	  {
//	    uchar vol = m_volPtr[d*m_width*m_height + w*m_height + h];
//	    if (lut[4*vol+3] > 10)
//	      m_voxels << d << w << h << vol;
//	  }
//      }
//  h=m_maxHSlice;
//  for(int d=m_minDSlice; d<m_maxDSlice; d+=m_pointSkip)
//    for(int w=m_minWSlice; w<m_maxWSlice; w+=m_pointSkip)
//      {
//	if (!clip(d, w, h))
//	  {
//	    uchar vol = m_volPtr[d*m_width*m_height + w*m_height + h];
//	    if (lut[4*vol+3] > 10)
//	      m_voxels << d << w << h << vol;
//	  }
//      }
//  //----------------------------------
//
//
//  //----------------------------------
//  // now for the interior  
//  for(d=m_minDSlice+1; d<m_maxDSlice-1; d+=m_pointSkip)
//    {
//      progress.setValue(100*(float)(d-m_minDSlice)/(m_maxDSlice-m_minDSlice));
//      qApp->processEvents();
//      for(w=m_minWSlice+1; w<m_maxWSlice-1; w+=m_pointSkip)
//	{
//	  for(h=m_minHSlice+1; h<m_maxHSlice-1; h+=m_pointSkip)
//	    {
//	      if (!clip(d, w, h))
//		{
//		  uchar vol = m_volPtr[d*m_width*m_height + w*m_height + h];
//		  if (lut[4*vol+3] > 10)
//		    {
//		      bool ok = false;
//		      for(int dd=-m_pointSkip; dd<=m_pointSkip; dd++)
//			for(int ww=-m_pointSkip; ww<=m_pointSkip; ww++)
//			  for(int hh=-m_pointSkip; hh<=m_pointSkip; hh++)
//			    {
//			      int d1 = qBound(m_minDSlice, d+dd, m_maxDSlice);
//			      int w1 = qBound(m_minWSlice, w+ww, m_maxWSlice);
//			      int h1 = qBound(m_minHSlice, h+hh, m_maxHSlice);
//			      
//			      uchar v = m_volPtr[d1*m_width*m_height + w1*m_height + h1];
//			      if (lut[4*v+3] < 10)
//				{
//				  ok = true;
//				  break;
//				}
//			    }
//		      
//		      if (ok)
//			m_voxels << d << w << h << vol;
//		    }
//		}
//	    }
//	}
//    }
  
  //progress.setValue(100);

    
  progress.setValue(10);

  if (!m_lutTex) glGenTextures(1, &m_lutTex);
  if (!m_tagTex) glGenTextures(1, &m_tagTex);

  qint64 dsz = (m_maxDSlice-m_minDSlice);
  qint64 wsz = (m_maxWSlice-m_minWSlice);
  qint64 hsz = (m_maxHSlice-m_minHSlice);
  qint64 tsz = dsz*wsz*hsz;

  m_sslevel = 1;
  while (tsz/1024.0/1024.0 > m_memSize ||
	 dsz > m_max3DTexSize ||
	 wsz > m_max3DTexSize ||
	 hsz > m_max3DTexSize)
    {
      m_sslevel++;
      dsz = (m_maxDSlice-m_minDSlice)/m_sslevel;
      wsz = (m_maxWSlice-m_minWSlice)/m_sslevel;
      hsz = (m_maxHSlice-m_minHSlice)/m_sslevel;

      if (dsz*m_sslevel < m_maxDSlice-m_minDSlice) dsz++;
      if (wsz*m_sslevel < m_maxWSlice-m_minWSlice) wsz++;
      if (hsz*m_sslevel < m_maxHSlice-m_minHSlice) hsz++;

      tsz = dsz*wsz*hsz;      
    }


  //-------------------------
  m_sslevel = QInputDialog::getInt(0,
				   "Subsampling Level",
				   "Subsampling Level",
				    m_sslevel, m_sslevel, 5, 1);

  dsz = (m_maxDSlice-m_minDSlice)/m_sslevel;
  wsz = (m_maxWSlice-m_minWSlice)/m_sslevel;
  hsz = (m_maxHSlice-m_minHSlice)/m_sslevel;
  if (dsz*m_sslevel < m_maxDSlice-m_minDSlice) dsz++;
  if (wsz*m_sslevel < m_maxWSlice-m_minWSlice) wsz++;
  if (hsz*m_sslevel < m_maxHSlice-m_minHSlice) hsz++;
  tsz = dsz*wsz*hsz;      
  //-------------------------

  m_corner = Vec(m_minHSlice, m_minWSlice, m_minDSlice);
  m_vsize = Vec(hsz, wsz, dsz);

  createRaycastShader();

  uchar *voxelVol = new uchar[tsz];

  //----------------------------
  // load data volume
  progress.setValue(20);
  int i = 0;
  for(int d=m_minDSlice; d<m_maxDSlice; d+=m_sslevel)
    for(int w=m_minWSlice; w<m_maxWSlice; w+=m_sslevel)
      for(int h=m_minHSlice; h<m_maxHSlice; h+=m_sslevel)
	{
	  voxelVol[i] = m_volPtr[d*m_width*m_height + w*m_height + h];
	  i++;
	}
  progress.setValue(50);

  if (!m_dataTex) glGenTextures(1, &m_dataTex);
  glActiveTexture(GL_TEXTURE2);
  glEnable(GL_TEXTURE_3D);
  glBindTexture(GL_TEXTURE_3D, m_dataTex);	 
  glTexParameterf(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); 
  glTexParameterf(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE); 
  glTexParameterf(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE); 
  glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  progress.setValue(70);
  glTexImage3D(GL_TEXTURE_3D,
	       0, // single resolution
	       1,
	       hsz, wsz, dsz,
	       0, // no border
	       GL_LUMINANCE,
	       GL_UNSIGNED_BYTE,
	       voxelVol);
  glDisable(GL_TEXTURE_3D);
  //----------------------------


  //----------------------------
  // load mask volume
  progress.setValue(60);
  i = 0;
  for(int d=m_minDSlice; d<m_maxDSlice; d+=m_sslevel)
    for(int w=m_minWSlice; w<m_maxWSlice; w+=m_sslevel)
      for(int h=m_minHSlice; h<m_maxHSlice; h+=m_sslevel)
	{
	  voxelVol[i] = m_maskPtr[d*m_width*m_height + w*m_height + h];
	  i++;
	}
  progress.setValue(80);

  if (!m_maskTex) glGenTextures(1, &m_maskTex);
  glActiveTexture(GL_TEXTURE4);
  glEnable(GL_TEXTURE_3D);
  glBindTexture(GL_TEXTURE_3D, m_maskTex);	 
  glTexParameterf(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); 
  glTexParameterf(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE); 
  glTexParameterf(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE); 
  glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  progress.setValue(70);
  glTexImage3D(GL_TEXTURE_3D,
	       0, // single resolution
	       1,
	       hsz, wsz, dsz,
	       0, // no border
	       GL_LUMINANCE,
	       GL_UNSIGNED_BYTE,
	       voxelVol);
  glDisable(GL_TEXTURE_3D);
  //----------------------------


  delete [] voxelVol;
  
  progress.setValue(100);

  update();
}

void
Viewer::drawVol()
{
  if (!m_volPtr || !m_maskPtr || m_pointSkip == 0)
    return;

  uchar *lut = Global::lut();

  Vec bmin, bmax;
  m_boundingBox.bounds(bmin, bmax);

  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glEnable(GL_POINT_SMOOTH);

  glEnable(GL_POINT_SPRITE_ARB);
  glEnable(GL_TEXTURE_2D);
  glBindTexture(GL_TEXTURE_2D, Global::spriteTexture());
  glTexEnvi(GL_POINT_SPRITE, GL_COORD_REPLACE, GL_TRUE);

  glBegin(GL_POINTS);

  int nv = m_voxels.count()/4;
  for(int i=0; i<nv; i++)
    {
      qint64 d = m_voxels[4*i+0];
      int w = m_voxels[4*i+1];
      int h = m_voxels[4*i+2];

      qint64 idx = d*m_width*m_height + w*m_height + h;
      if (d < bmin.z || d > bmax.z ||
	  w < bmin.y || w > bmax.y ||
	  h < bmin.x || h > bmax.x ||
	  !m_bitmask.testBit(idx)) // carved
	{}
      else
	{
	  int v = m_voxels[4*i+3];
	  int t = m_maskPtr[d*m_width*m_height + w*m_height + h];
	  
	  float r = lut[4*v+2]*1.0/255.0;
	  float g = lut[4*v+1]*1.0/255.0;
	  float b = lut[4*v+0]*1.0/255.0;
	  
	  float rt = r;
	  float gt = g;
	  float bt = b;
	  if (t > 0)
	    {
	      rt = Global::tagColors()[4*t+0]*1.0/255.0;
	      gt = Global::tagColors()[4*t+1]*1.0/255.0;
	      bt = Global::tagColors()[4*t+2]*1.0/255.0;
	    }
	  
	  r = r*0.5 + rt*0.5;
	  g = g*0.5 + gt*0.5;
	  b = b*0.5 + bt*0.5;
	  
	  glColor3f(r,g,b);
	  glVertex3f(h, w, d);
	}
    }

  glEnd();

  glDisable(GL_POINT_SPRITE);
  glDisable(GL_TEXTURE_2D);

  glDisable(GL_POINT_SMOOTH);
  glBlendFunc(GL_NONE, GL_NONE);
  glDisable(GL_BLEND);
}


void
Viewer::drawMMDCurve()
{
  if (!m_Dcg) return;

  QList<int> cgkeys = m_Dcg->uniqueKeys();
  for(int i=0; i<cgkeys.count(); i++)
    {
      QList<Curve*> curves = m_Dcg->values(cgkeys[i]);
      for (int j=0; j<curves.count(); j++)
	drawCurve(0, curves[j], cgkeys[i]);
    }
  glLineWidth(1);
}

void
Viewer::drawMMWCurve()
{
  if (!m_Wcg) return;

  QList<int> cgkeys = m_Wcg->uniqueKeys();
  for(int i=0; i<cgkeys.count(); i++)
    {
      QList<Curve*> curves = m_Wcg->values(cgkeys[i]);
      for (int j=0; j<curves.count(); j++)
	drawCurve(1, curves[j], cgkeys[i]);
    }
  glLineWidth(1);
}

void
Viewer::drawMMHCurve()
{
  if (!m_Hcg) return;

  QList<int> cgkeys = m_Hcg->uniqueKeys();
  for(int i=0; i<cgkeys.count(); i++)
    {
      QList<Curve*> curves = m_Hcg->values(cgkeys[i]);
      for (int j=0; j<curves.count(); j++)
	drawCurve(2, curves[j], cgkeys[i]);
    }
  glLineWidth(1);
}

void
Viewer::drawLMDCurve()
{
  if (!m_Dmcg) return;

  for(int i=0; i<m_Dmcg->count(); i++)
    {
      QMap<int, Curve> mcg = m_Dmcg->at(i);
      QList<int> cgkeys = mcg.keys();
      for(int j=0; j<cgkeys.count(); j++)
	{
	  Curve c = mcg.value(cgkeys[j]);
	  drawCurve(0, &c, cgkeys[j]);
	}
    }
  glLineWidth(1);
}

void
Viewer::drawLMWCurve()
{
  if (!m_Wmcg) return;

  for(int i=0; i<m_Wmcg->count(); i++)
    {
      QMap<int, Curve> mcg = m_Wmcg->at(i);
      QList<int> cgkeys = mcg.keys();
      for(int j=0; j<cgkeys.count(); j++)
	{
	  Curve c = mcg.value(cgkeys[j]);
	  drawCurve(1, &c, cgkeys[j]);
	}
    }
  glLineWidth(1);
}

void
Viewer::drawLMHCurve()
{
  if (!m_Hmcg) return;

  for(int i=0; i<m_Hmcg->count(); i++)
    {
      QMap<int, Curve> mcg = m_Hmcg->at(i);
      QList<int> cgkeys = mcg.keys();
      for(int j=0; j<cgkeys.count(); j++)
	{
	  Curve c = mcg.value(cgkeys[j]);
	  drawCurve(2, &c, cgkeys[j]);
	}
    }
  glLineWidth(1);
}

void
Viewer::setDSlice(int d)
{
  m_dslice = d;
  m_dvoxels.clear();

  if (m_volPtr && m_dslice >= 0)
    {
      uchar *lut = Global::lut();  
      int d=m_dslice;
//      for(int w=m_minWSlice; w<m_maxWSlice; w+=m_pointSkip)
//	for(int h=m_minHSlice; h<m_maxHSlice; h+=m_pointSkip)
      for(int w=m_minWSlice; w<m_maxWSlice; w++)
	for(int h=m_minHSlice; h<m_maxHSlice; h++)
	  {
	    uchar v = m_volPtr[d*m_width*m_height + w*m_height + h];
	    if (lut[4*v+3] > 10)
	      {
		int r = lut[4*v+2];
		int g = lut[4*v+1];
		int b = lut[4*v+0];
		m_dvoxels << w << h << r << g << b;
	      }
	  }
    }      

  update();
}

void
Viewer::setWSlice(int w)
{
  m_wslice = w;
  m_wvoxels.clear();

  if (m_volPtr && m_wslice >= 0)
    {
      uchar *lut = Global::lut();  
      int w=m_wslice;
//      for(int d=m_minDSlice; d<m_maxDSlice; d+=m_pointSkip)
//	for(int h=m_minHSlice; h<m_maxHSlice; h+=m_pointSkip)
      for(int d=m_minDSlice; d<m_maxDSlice; d++)
	for(int h=m_minHSlice; h<m_maxHSlice; h++)
	  {
	    uchar v = m_volPtr[d*m_width*m_height + w*m_height + h];
	    if (lut[4*v+3] > 10)
	      {
		int r = lut[4*v+2];
		int g = lut[4*v+1];
		int b = lut[4*v+0];
		m_wvoxels << d << h << r << g << b;
	      }
	  }
    }      

  update();
}

void
Viewer::setHSlice(int h)
{
  m_hslice = h;
  m_hvoxels.clear();

  if (m_volPtr && m_hslice >= 0)
    {
      uchar *lut = Global::lut();  
      int h=m_hslice;
//      for(int d=m_minDSlice; d<m_maxDSlice; d+=m_pointSkip)
//	for(int w=m_minWSlice; w<m_maxWSlice; w+=m_pointSkip)
      for(int d=m_minDSlice; d<m_maxDSlice; d++)
	for(int w=m_minWSlice; w<m_maxWSlice; w++)
	  {
	    uchar v = m_volPtr[d*m_width*m_height + w*m_height + h];
	    if (lut[4*v+3] > 10)
	      {
		int r = lut[4*v+2];
		int g = lut[4*v+1];
		int b = lut[4*v+0];
		m_hvoxels << d << w << r << g << b;
	      }
	  }
    }      

  update();
}

void
Viewer::drawSlices()
{
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glEnable(GL_POINT_SMOOTH);

//  glEnable(GL_POINT_SPRITE_ARB);
//  glEnable(GL_TEXTURE_2D);
//  glBindTexture(GL_TEXTURE_2D, Global::spriteTexture());
//  glTexEnvi(GL_POINT_SPRITE, GL_COORD_REPLACE, GL_TRUE);

  //glPointSize(m_pointSize);
  Vec ptpos = camera()->sceneCenter();
  float pglr = camera()->pixelGLRatio(ptpos);
  int ptsz = m_pointSize/pglr;
  glPointSize(ptsz);

  for(int i=0; i<m_dvoxels.count()/5; i++)
    {
      int w = m_dvoxels[5*i+0];
      int h = m_dvoxels[5*i+1];
      float r = (float)m_dvoxels[5*i+2]/255.0;
      float g = (float)m_dvoxels[5*i+3]/255.0;
      float b = (float)m_dvoxels[5*i+4]/255.0;
      glBegin(GL_POINTS);
      glColor3f(r,g,b);
      glVertex3f(h, w, m_dslice);
      glEnd();
    }

  for(int i=0; i<m_wvoxels.count()/5; i++)
    {
      int d = m_wvoxels[5*i+0];
      int h = m_wvoxels[5*i+1];
      float r = (float)m_wvoxels[5*i+2]/255.0;
      float g = (float)m_wvoxels[5*i+3]/255.0;
      float b = (float)m_wvoxels[5*i+4]/255.0;
      glBegin(GL_POINTS);
      glColor3f(r,g,b);
      glVertex3f(h, m_wslice, d);
      glEnd();
    }

  for(int i=0; i<m_hvoxels.count()/5; i++)
    {
      int d = m_hvoxels[5*i+0];
      int w = m_hvoxels[5*i+1];
      float r = (float)m_hvoxels[5*i+2]/255.0;
      float g = (float)m_hvoxels[5*i+3]/255.0;
      float b = (float)m_hvoxels[5*i+4]/255.0;
      glBegin(GL_POINTS);
      glColor3f(r,g,b);
      glVertex3f(m_hslice, w, d);
      glEnd();
    }

//  glDisable(GL_POINT_SPRITE);
//  glDisable(GL_TEXTURE_2D);

  glDisable(GL_POINT_SMOOTH);
  glBlendFunc(GL_NONE, GL_NONE);
  glDisable(GL_BLEND);
}

void
Viewer::drawCurve(int type, Curve *c, int slc)
{
  int tag = c->tag;
  if (m_curveTags.count() == 0 ||
      m_curveTags[0] == -1 ||
      m_curveTags.contains(tag))
    {
      float r = Global::tagColors()[4*tag+0]*1.0/255.0;
      float g = Global::tagColors()[4*tag+1]*1.0/255.0;
      float b = Global::tagColors()[4*tag+2]*1.0/255.0;
      glColor3f(r,g,b);
      glLineWidth(c->thickness);
      if (type == 0)
	{
	  glBegin(GL_LINE_STRIP);
	  for(int k=0; k<c->pts.count(); k++)
	    glVertex3f(c->pts[k].x(), c->pts[k].y(), slc);
	  if (c->closed)
	    glVertex3f(c->pts[0].x(), c->pts[0].y(), slc);
	  glEnd();
	}
      else if (type == 1)
	{
	  glBegin(GL_LINE_STRIP);
	  for(int k=0; k<c->pts.count(); k++)
	    glVertex3f(c->pts[k].x(), slc, c->pts[k].y());
	  if (c->closed)
	    glVertex3f(c->pts[0].x(), slc, c->pts[0].y());
	  glEnd();
	}
      else if (type == 2)
	{
	  glBegin(GL_LINE_STRIP);
	  for(int k=0; k<c->pts.count(); k++)
	    glVertex3f(slc, c->pts[k].x(), c->pts[k].y());
	  if (c->closed)
	    glVertex3f(slc, c->pts[0].x(), c->pts[0].y());
	  glEnd();
	}
    }
}

void
Viewer::drawSWDCurve()
{
  if (!m_Dswcg) return;

  for(int ix=0; ix<m_Dswcg->count(); ix++)
    {
      QMultiMap<int, Curve*> mcg = m_Dswcg->at(ix);

      QList<int> cgkeys = mcg.uniqueKeys();
      for(int i=0; i<cgkeys.count(); i++)
	{
	  QList<Curve*> curves = mcg.values(cgkeys[i]);
	  for (int j=0; j<curves.count(); j++)
	    drawCurve(0, curves[j], cgkeys[i]);
	}
    }

  glLineWidth(1);
}

void
Viewer::drawSWWCurve()
{
  if (!m_Wswcg) return;

  for(int ix=0; ix<m_Wswcg->count(); ix++)
    {
      QMultiMap<int, Curve*> mcg = m_Wswcg->at(ix);

      QList<int> cgkeys = mcg.uniqueKeys();
      for(int i=0; i<cgkeys.count(); i++)
	{
	  QList<Curve*> curves = mcg.values(cgkeys[i]);
	  for (int j=0; j<curves.count(); j++)
	    drawCurve(1, curves[j], cgkeys[i]);
	}
    }

  glLineWidth(1);
}

void
Viewer::drawSWHCurve()
{
  if (!m_Hswcg) return;

  for(int ix=0; ix<m_Hswcg->count(); ix++)
    {
      QMultiMap<int, Curve*> mcg = m_Hswcg->at(ix);

      QList<int> cgkeys = mcg.uniqueKeys();
      for(int i=0; i<cgkeys.count(); i++)
	{
	  QList<Curve*> curves = mcg.values(cgkeys[i]);
	  for (int j=0; j<curves.count(); j++)
	    drawCurve(2, curves[j], cgkeys[i]);
	}
    }

  glLineWidth(1);
}

void
Viewer::getHit(QMouseEvent *event)
{
  bool found;
  QPoint scr = event->pos();
  
  m_target = pointUnderPixel_RC(scr, found);

  if (found)
    {
      int d, w, h;
      d = m_target.z;
      w = m_target.y;
      h = m_target.x;

      int b = 0;
      if (event->buttons() == Qt::LeftButton) b = 1;
      else if (event->buttons() == Qt::RightButton) b = 2;
      else if (event->buttons() == Qt::MiddleButton) b = 3;
      
      if (m_paintHit)
	emit paint3D(d, w, h, b, Global::tag(), m_sslevel);      
      else if (m_carveHit)
	carve(d, w, h, b==2);
    }
}

Vec
Viewer::pointUnderPixel(QPoint scr, bool& found)
{
  int sw = camera()->screenWidth();
  int sh = camera()->screenHeight();  

  Vec pos;
  int cx = scr.x();
  int cy = scr.y();
  GLfloat depth = 0;
  
  glEnable(GL_SCISSOR_TEST);
  glScissor(cx, sh-1-cy, 1, 1);
  drawPointsWithoutShader();
  glDisable(GL_SCISSOR_TEST);

  glReadPixels(cx, sh-1-cy, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &depth);

  found = false;
  if (depth > 0.0 && depth < 1.0)
    {
      pos = camera()->unprojectedCoordinatesOf(Vec(cx, cy, depth));
      found = true;
    }

  return pos;
}

Vec
Viewer::pointUnderPixel_RC(QPoint scr, bool& found)
{
  found = false;
      
  Vec box[8];
  box[0] = Vec(m_minHSlice, m_minWSlice, m_minDSlice);
  box[1] = Vec(m_minHSlice, m_minWSlice, m_maxDSlice);
  box[2] = Vec(m_minHSlice, m_maxWSlice, m_maxDSlice);
  box[3] = Vec(m_minHSlice, m_maxWSlice, m_minDSlice);
  box[4] = Vec(m_maxHSlice, m_minWSlice, m_minDSlice);
  box[5] = Vec(m_maxHSlice, m_minWSlice, m_maxDSlice);
  box[6] = Vec(m_maxHSlice, m_maxWSlice, m_maxDSlice);
  box[7] = Vec(m_maxHSlice, m_maxWSlice, m_minDSlice);

  //--------------------------------
  Vec eyepos = camera()->position();
  Vec viewDir = camera()->viewDirection();
  float minZ = 1000000;
  float maxZ = -1000000;
  for(int b=0; b<8; b++)
    {
      float zv = (box[b]-eyepos)*viewDir;
      minZ = qMin(minZ, zv);
      maxZ = qMax(maxZ, zv);
    }
  //--------------------------------

  int sw = camera()->screenWidth();
  int sh = camera()->screenHeight();  

  Vec pos;
  int cx = scr.x();
  int cy = scr.y();
  GLfloat depth = 0;
  
  glEnable(GL_SCISSOR_TEST);
  glScissor(cx, sh-cy, 1, 1);

  volumeRaycast(minZ, maxZ, true); // run only one part of raycast process

  glBindFramebuffer(GL_FRAMEBUFFER, m_slcBuffer);
  glBindTexture(GL_TEXTURE_RECTANGLE_ARB, m_slcTex[0]);

  glDisable(GL_SCISSOR_TEST);

  GLfloat d4[4];
  glReadPixels(cx, sh-cy, 1, 1, GL_RGBA, GL_FLOAT, &d4);
  if (d4[3] > 0.0)
    {
      pos = Vec(d4[0], d4[1], d4[2]);
      Vec vsz = m_sslevel*m_vsize;
      pos = m_corner + VECPRODUCT(pos, vsz);
      found = true;
    }

  glBindFramebuffer(GL_FRAMEBUFFER, 0);

  return pos;
}

void
Viewer::mousePressEvent(QMouseEvent *event)
{
  m_paintHit = false;
  m_carveHit = false;
  m_target = Vec(-1,-1,-1);

  if (event->modifiers() & Qt::ShiftModifier)
    {
      m_paintHit = true;
      getHit(event);
      return;
    }

  if (event->modifiers() & Qt::ControlModifier)
    {
      m_carveHit = true;
      getHit(event);
      return;
    }

  QGLViewer::mousePressEvent(event);
}


void
Viewer::mouseReleaseEvent(QMouseEvent *event)
{
  m_dragMode = false;

  if (m_paintHit)
    emit paint3DEnd();
  
  m_carveHit = false;
  m_paintHit = false;
  m_target = Vec(-1,-1,-1);

  QGLViewer::mouseReleaseEvent(event);
}


void
Viewer::mouseMoveEvent(QMouseEvent *event)
{
  m_dragMode = event->buttons() != Qt::NoButton;

  m_target = Vec(-1,-1,-1);

  if (m_paintHit || m_carveHit)
    {
      getHit(event);
      return;
    }

  QGLViewer::mouseMoveEvent(event);
}

void
Viewer::drawClip()
{
  updateClipVoxels();

  //m_clipPlanes->draw(this, false);

  uchar *lut = Global::lut();

  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glEnable(GL_POINT_SMOOTH);

  Vec ptpos = camera()->sceneCenter();
  float pglr = camera()->pixelGLRatio(ptpos);
  int ptsz = m_pointSize/pglr;
  glPointSize(ptsz);
  int nv = m_clipVoxels.count()/4;
  for(int i=0; i<nv; i++)
    {
      int d = m_clipVoxels[4*i+0];
      int w = m_clipVoxels[4*i+1];
      int h = m_clipVoxels[4*i+2];
      int v = m_clipVoxels[4*i+3];

      int t = m_maskPtr[d*m_width*m_height + w*m_height + h];

      glBegin(GL_POINTS);
      float r = lut[4*v+2]*1.0/255.0;
      float g = lut[4*v+1]*1.0/255.0;
      float b = lut[4*v+0]*1.0/255.0;

      float rt = r;
      float gt = g;
      float bt = b;
      if (t > 0)
	{
	  rt = Global::tagColors()[4*t+0]*1.0/255.0;
	  gt = Global::tagColors()[4*t+1]*1.0/255.0;
	  bt = Global::tagColors()[4*t+2]*1.0/255.0;
	}

      r = r*0.5 + rt*0.5;
      g = g*0.5 + gt*0.5;
      b = b*0.5 + bt*0.5;

      glColor3f(r,g,b);
      glVertex3f(h, w, d);
      glEnd();
    }

  glDisable(GL_POINT_SMOOTH);
  glBlendFunc(GL_NONE, GL_NONE);
  glDisable(GL_BLEND);
}


void
Viewer::carve(int d, int w, int h, bool b)
{
  if (d<0 || w<0 || h<0 ||
      d>m_depth-1 ||
      w>m_width-1 ||
      h>m_height-1)
    return;

  int rad = Global::spread();
  
  int ds, de, ws, we, hs, he;
  ds = qMax(m_minDSlice, d-rad);
  de = qMin(m_maxDSlice, d+rad);
  ws = qMax(m_minWSlice, w-rad);
  we = qMin(m_maxWSlice, w+rad);
  hs = qMax(m_minHSlice, h-rad);
  he = qMin(m_maxHSlice, h+rad);
  for(qint64 dd=ds; dd<=de; dd++)
    for(int ww=ws; ww<=we; ww++)
      for(int hh=hs; hh<=he; hh++)
	{
	  float p = ((d-dd)*(d-dd)+
		     (w-ww)*(w-ww)+
		     (h-hh)*(h-hh));
	  if (p < rad*rad)
	    {
	      qint64 idx = dd*m_width*m_height + ww*m_height + hh;
	      m_bitmask.setBit(idx, b); 
	    }
	}

  update();
}

void
Viewer::drawClipSlices()
{
  Vec box[8];
  box[0] = Vec(m_minHSlice, m_minWSlice, m_minDSlice);
  box[1] = Vec(m_minHSlice, m_minWSlice, m_maxDSlice);
  box[2] = Vec(m_minHSlice, m_maxWSlice, m_maxDSlice);
  box[3] = Vec(m_minHSlice, m_maxWSlice, m_minDSlice);
  box[4] = Vec(m_maxHSlice, m_minWSlice, m_minDSlice);
  box[5] = Vec(m_maxHSlice, m_minWSlice, m_maxDSlice);
  box[6] = Vec(m_maxHSlice, m_maxWSlice, m_maxDSlice);
  box[7] = Vec(m_maxHSlice, m_maxWSlice, m_minDSlice);

  QList<Vec> cPos =  m_clipPlanes->positions();
  QList<Vec> cNorm = m_clipPlanes->normals();


  glUseProgramObjectARB(m_sliceShader);
  glUniform1iARB(m_sliceParm[0], 2); // dataTex
  glUniform1iARB(m_sliceParm[1], 3); // lutTex


  glActiveTexture(GL_TEXTURE2);
  glEnable(GL_TEXTURE_3D);
  glBindTexture(GL_TEXTURE_3D, m_dataTex);

  uchar *lut = Global::lut();
  glActiveTexture(GL_TEXTURE3);
  glEnable(GL_TEXTURE_2D);
  glBindTexture(GL_TEXTURE_2D, m_lutTex);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); 
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE); 
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,GL_NEAREST);
  glTexImage2D(GL_TEXTURE_2D,
	       0, // single resolution
	       GL_RGBA,
	       256, Global::lutSize()*256, // width, height
	       0, // no border
	       GL_BGRA,
	       GL_UNSIGNED_BYTE,
	       lut);

  for(int i=0; i<cPos.count(); i++)
    {
      Vec cpos = cPos[i];
      Vec cnorm = cNorm[i];

      drawPoly(cpos, cnorm, box);
    }

  glUseProgramObjectARB(0);

  glActiveTexture(GL_TEXTURE3);
  glDisable(GL_TEXTURE_2D);

  glActiveTexture(GL_TEXTURE2);
  glDisable(GL_TEXTURE_3D);
}

int
Viewer::drawPoly(Vec po, Vec pn, Vec *subvol)
{
  Vec poly[20];
  int edges = 0;

  edges += StaticFunctions::intersectType1(po, pn,  subvol[0], subvol[1], poly[edges]);
  edges += StaticFunctions::intersectType1(po, pn,  subvol[0], subvol[3], poly[edges]);
  edges += StaticFunctions::intersectType1(po, pn,  subvol[2], subvol[1], poly[edges]);
  edges += StaticFunctions::intersectType1(po, pn,  subvol[2], subvol[3], poly[edges]);
  edges += StaticFunctions::intersectType1(po, pn,  subvol[4], subvol[5], poly[edges]);
  edges += StaticFunctions::intersectType1(po, pn,  subvol[4], subvol[7], poly[edges]);
  edges += StaticFunctions::intersectType1(po, pn,  subvol[6], subvol[5], poly[edges]);
  edges += StaticFunctions::intersectType1(po, pn,  subvol[6], subvol[7], poly[edges]);
  edges += StaticFunctions::intersectType1(po, pn,  subvol[0], subvol[4], poly[edges]);
  edges += StaticFunctions::intersectType1(po, pn,  subvol[1], subvol[5], poly[edges]);
  edges += StaticFunctions::intersectType1(po, pn,  subvol[2], subvol[6], poly[edges]);
  edges += StaticFunctions::intersectType1(po, pn,  subvol[3], subvol[7], poly[edges]);

  if (!edges) return 0;

  Vec cen;
  int i;
  for(i=0; i<edges; i++)
    cen += poly[i];
  cen/=edges;

  float angle[12];
  Vec vaxis, vperp;
  vaxis = poly[0]-cen;
  vaxis.normalize();

  vperp = vaxis^(poly[1]-cen);
  vperp = vperp^vaxis;
  vperp.normalize();

  angle[0] = 1;
  for(i=1; i<edges; i++)
    {
      Vec v;
      v = poly[i]-cen;
      v.normalize();
      angle[i] = vaxis*v;
      if (vperp*v < 0)
	angle[i] = -2 - angle[i];
    }

  // sort angle
  int order[] = {0, 1, 2, 3, 4, 5 };
  for(i=edges-1; i>=0; i--)
    for(int j=1; j<=i; j++)
      {
	if (angle[order[i]] < angle[order[j]])
	  {
	    int tmp = order[i];
	    order[i] = order[j];
	    order[j] = tmp;
	  }
      }

  glBegin(GL_POLYGON);
  for(i=0; i<edges; i++)
    {  
      Vec tx, tc;
      Vec p = poly[order[i]];
      tx = p-m_corner;
      tx /= m_sslevel;
      tx = VECDIVIDE(tx,m_vsize);

      glTexCoord3f(tx.x, tx.y, tx.z);
      glVertex3f(p.x, p.y, p.z);
    }
  glEnd();


  return 1;
}

//void
//Viewer::drawBox(GLenum glFaces)
//{
//  Vec box[8];
////  box[0] = Vec(m_minHSlice, m_minWSlice, m_minDSlice);
////  box[1] = Vec(m_minHSlice, m_minWSlice, m_maxDSlice);
////  box[2] = Vec(m_minHSlice, m_maxWSlice, m_minDSlice);
////  box[3] = Vec(m_minHSlice, m_maxWSlice, m_maxDSlice);
////  box[4] = Vec(m_maxHSlice, m_minWSlice, m_minDSlice);
////  box[5] = Vec(m_maxHSlice, m_minWSlice, m_maxDSlice);
////  box[6] = Vec(m_maxHSlice, m_maxWSlice, m_minDSlice);
////  box[7] = Vec(m_maxHSlice, m_maxWSlice, m_maxDSlice);
//
//  Vec bmin, bmax;
//  m_boundingBox.bounds(bmin, bmax);
//
//  bmin = StaticFunctions::maxVec(bmin, Vec(m_minHSlice, m_minWSlice, m_minDSlice));
//  bmax = StaticFunctions::minVec(bmax, Vec(m_maxHSlice, m_maxWSlice, m_maxDSlice));
//
//  box[0] = Vec(bmin.x,bmin.y,bmin.z);
//  box[1] = Vec(bmin.x,bmin.y,bmax.z);
//  box[2] = Vec(bmin.x,bmax.y,bmin.z);
//  box[3] = Vec(bmin.x,bmax.y,bmax.z);
//  box[4] = Vec(bmax.x,bmin.y,bmin.z);
//  box[5] = Vec(bmax.x,bmin.y,bmax.z);
//  box[6] = Vec(bmax.x,bmax.y,bmin.z);
//  box[7] = Vec(bmax.x,bmax.y,bmax.z);
//
//  float xmin, xmax, ymin, ymax, zmin, zmax;
//  xmin = (bmin.x-m_minHSlice)/(m_maxHSlice-m_minHSlice);
//  xmax = (bmax.x-m_minHSlice)/(m_maxHSlice-m_minHSlice);
//  ymin = (bmin.y-m_minWSlice)/(m_maxWSlice-m_minWSlice);
//  ymax = (bmax.y-m_minWSlice)/(m_maxWSlice-m_minWSlice);
//  zmin = (bmin.z-m_minDSlice)/(m_maxDSlice-m_minDSlice);
//  zmax = (bmax.z-m_minDSlice)/(m_maxDSlice-m_minDSlice);
//
//  Vec col[8];
////  col[0] = Vec(0,0,0);
////  col[1] = Vec(0,0,1);
////  col[2] = Vec(0,1,0);
////  col[3] = Vec(0,1,1);
////  col[4] = Vec(1,0,0);
////  col[5] = Vec(1,0,1);
////  col[6] = Vec(1,1,0);
////  col[7] = Vec(1,1,1);
//
//  col[0] = Vec(xmin,ymin,zmin);
//  col[1] = Vec(xmin,ymin,zmax);
//  col[2] = Vec(xmin,ymax,zmin);
//  col[3] = Vec(xmin,ymax,zmax);
//  col[4] = Vec(xmax,ymin,zmin);
//  col[5] = Vec(xmax,ymin,zmax);
//  col[6] = Vec(xmax,ymax,zmin);
//  col[7] = Vec(xmax,ymax,zmax);
//
//// front: 1 5 7 3
//// back: 0 2 6 4
//// left:0 1 3 2
//// right:7 5 4 6    
//// up: 2 3 7 6
//// down: 1 0 4 5
//
//  int faces[] = {1, 5, 7, 3,
//		 0, 2, 6, 4,
//		 0, 1, 3, 2,
//		 7, 5, 4, 6,
//		 2, 3, 7, 6,
//		 1, 0, 4, 5};
//
//
//  glEnable(GL_CULL_FACE);
//  glCullFace(glFaces);
//
//  for(int i=0; i<6; i++)
//    {
//      glBegin(GL_QUADS);
//      for(int j=0; j<4; j++)
//	{
//	  int idx = faces[4*i+j];
//	  glColor3f(col[idx].x, col[idx].y, col[idx].z);
//	  glVertex3f(box[idx].x,box[idx].y, box[idx].z);
//	}
//      glEnd();
//    }
//
//  glDisable(GL_CULL_FACE);
//}

void
Viewer::drawBox(GLenum glFaces)
{
  Vec box[8];

  Vec bmin, bmax;
  m_boundingBox.bounds(bmin, bmax);

  bmin = StaticFunctions::maxVec(bmin, Vec(m_minHSlice, m_minWSlice, m_minDSlice));
  bmax = StaticFunctions::minVec(bmax, Vec(m_maxHSlice, m_maxWSlice, m_maxDSlice));

  box[0] = Vec(bmin.x,bmin.y,bmin.z);
  box[1] = Vec(bmin.x,bmin.y,bmax.z);
  box[2] = Vec(bmin.x,bmax.y,bmin.z);
  box[3] = Vec(bmin.x,bmax.y,bmax.z);
  box[4] = Vec(bmax.x,bmin.y,bmin.z);
  box[5] = Vec(bmax.x,bmin.y,bmax.z);
  box[6] = Vec(bmax.x,bmax.y,bmin.z);
  box[7] = Vec(bmax.x,bmax.y,bmax.z);

  float xmin, xmax, ymin, ymax, zmin, zmax;
  xmin = (bmin.x-m_minHSlice)/(m_maxHSlice-m_minHSlice);
  xmax = (bmax.x-m_minHSlice)/(m_maxHSlice-m_minHSlice);
  ymin = (bmin.y-m_minWSlice)/(m_maxWSlice-m_minWSlice);
  ymax = (bmax.y-m_minWSlice)/(m_maxWSlice-m_minWSlice);
  zmin = (bmin.z-m_minDSlice)/(m_maxDSlice-m_minDSlice);
  zmax = (bmax.z-m_minDSlice)/(m_maxDSlice-m_minDSlice);

  Vec col[8];
  col[0] = Vec(xmin,ymin,zmin);
  col[1] = Vec(xmin,ymin,zmax);

  col[2] = Vec(xmin,ymax,zmin);
  col[3] = Vec(xmin,ymax,zmax);

  col[4] = Vec(xmax,ymin,zmin);
  col[5] = Vec(xmax,ymin,zmax);

  col[6] = Vec(xmax,ymax,zmin);
  col[7] = Vec(xmax,ymax,zmax);

  int faces[] = {1, 5, 7, 3,
		 0, 2, 6, 4,
		 0, 1, 3, 2,
		 7, 5, 4, 6,
		 2, 3, 7, 6,
		 1, 0, 4, 5};

  glEnable(GL_CULL_FACE);
  glCullFace(glFaces);

  for(int i=0; i<6; i++)
    {
      //glBegin(GL_QUADS);
      Vec poly[100];
      Vec tex[100];
      for(int j=0; j<4; j++)
	{
	  int idx = faces[4*i+j];
	  poly[j] = box[idx];
	  tex[j] = col[idx];
	  //glColor3f(col[idx].x, col[idx].y, col[idx].z);
	  //glVertex3f(box[idx].x,box[idx].y, box[idx].z);
	}
      drawFace(4, &poly[0], &tex[0]);
      //glEnd();
    }

  drawClipFaces(&box[0], &col[0]);

  glDisable(GL_CULL_FACE);
}

void
Viewer::drawFace(int oedges, Vec *opoly, Vec *otex)
{
  QList<Vec> cPos =  m_clipPlanes->positions();
  QList<Vec> cNorm = m_clipPlanes->normals();

  cPos << camera()->position()+50*camera()->viewDirection();
  cNorm << -camera()->viewDirection();


  int edges = oedges;
  Vec poly[100];
  Vec tex[100];
  for(int i=0; i<edges; i++) poly[i] = opoly[i];
  for(int i=0; i<edges; i++) tex[i] = otex[i];

  //---- apply clipping
  for(int ci=0; ci<cPos.count(); ci++)
    {
      Vec cpo = cPos[ci];
      Vec cpn = cNorm[ci];
      
      int tedges = 0;
      Vec tpoly[100];
      Vec ttex[100];
      for(int i=0; i<edges; i++)
	{
	  Vec v0, v1, t0, t1;
	  
	  v0 = poly[i];
	  t0 = tex[i];
	  if (i<edges-1)
	    {
	      v1 = poly[i+1];
	      t1 = tex[i+1];
	    }
	  else
	    {
	      v1 = poly[0];
	      t1 = tex[0];
	    }
	  
	  int ret = StaticFunctions::intersectType2WithTexture(cpo, cpn,
							       v0, v1,
							       t0, t1);
	  if (ret)
	    {
	      tpoly[tedges] = v0;
	      ttex[tedges] = t0;
	      tedges ++;
	      if (ret == 2)
		{
		  tpoly[tedges] = v1;
		  ttex[tedges] = t1;
		  tedges ++;
		}
	    }
	}

      //QMessageBox::information(0, "", QString("%1").arg(tedges));

      edges = tedges;
      for(int i=0; i<edges; i++) poly[i] = tpoly[i];
      for(int i=0; i<edges; i++) tex[i] = ttex[i];
    }
  //---- clipping applied

  if (edges > 0)
    {
      glBegin(GL_POLYGON);
      for(int i=0; i<edges; i++)
	{
	  glColor3f(tex[i].x, tex[i].y, tex[i].z);
	  glVertex3f(poly[i].x, poly[i].y, poly[i].z);
	}
      glEnd();
    }  
}

void
Viewer::drawClipFaces(Vec *subvol, Vec *texture)
{
  int sidx[] = {0, 1,
		0, 2,
		0, 4,
		7, 5,
		7, 3,
		7, 6,
		1, 3,
		1, 5,
		2, 3,
		2, 6,
		4, 5,
		4, 6 };

  QList<Vec> cPos =  m_clipPlanes->positions();
  QList<Vec> cNorm = m_clipPlanes->normals();

  cPos << camera()->position()+50*camera()->viewDirection();
  cNorm << -camera()->viewDirection();

  for(int oci=0; oci<cPos.count(); oci++)
    {
      Vec cpo = cPos[oci];
      Vec cpn = cNorm[oci];

      Vec opoly[100];
      Vec otex[100];
      int edges = 0;

      QString mesg;
      for(int si=0; si<12; si++)
	{
	  int k = sidx[2*si];
	  int l = sidx[2*si+1];
	  edges += StaticFunctions::intersectType1WithTexture(cpo, cpn,
							      subvol[k], subvol[l],
							      texture[k], texture[l],
							      opoly[edges], otex[edges]);
	}

      if (edges > 2)
	{      
	  Vec cen;
	  int i;
	  for(i=0; i<edges; i++)
	    cen += opoly[i];
	  cen/=edges;
	  
	  float angle[12];
	  Vec vaxis, vperp;
	  vaxis = opoly[0]-cen;
	  vaxis.normalize();
	  
	  vperp = cpn ^ vaxis ;
	  
	  angle[0] = 1;
	  for(i=1; i<edges; i++)
	    {
	      Vec v;
	      v = opoly[i]-cen;
	      v.normalize();
	      angle[i] = vaxis*v;
	      if (vperp*v < 0)
		angle[i] = -2 - angle[i];
	    }
	  
	  // sort angle
	  int order[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
	  for(i=edges-1; i>=0; i--)
	    for(int j=1; j<=i; j++)
	      {
		if (angle[order[i]] > angle[order[j]])
		  {
		    int tmp = order[i];
		    order[i] = order[j];
		    order[j] = tmp;
		  }
	      }
	  
	  Vec poly[100];
	  Vec tex[100];
	  for(int i=0; i<edges; i++) poly[i] = opoly[order[i]];
	  for(int i=0; i<edges; i++) tex[i] = otex[order[i]];

	  //---- apply clipping
	  for(int ci=0; ci<cPos.count(); ci++)
	    {
	      if (ci != oci)
		{
		  Vec cpo = cPos[ci];
		  Vec cpn = cNorm[ci];
		  
		  int tedges = 0;
		  Vec tpoly[100];
		  Vec ttex[100];
		  for(int i=0; i<edges; i++)
		    {
		      Vec v0, v1, t0, t1;
		      
		      v0 = poly[i];
		      t0 = tex[i];
		      if (i<edges-1)
			{
			  v1 = poly[i+1];
			  t1 = tex[i+1];
			}
		      else
			{
			  v1 = poly[0];
			  t1 = tex[0];
			}
		      
		      // clip on texture coordinates
		      int ret = StaticFunctions::intersectType2WithTexture(cpo, cpn,
									   v0, v1,
									   t0, t1);
		      if (ret)
			{
			  tpoly[tedges] = v0;
			  ttex[tedges] = t0;
			  tedges ++;
			  if (ret == 2)
			    {
			      tpoly[tedges] = v1;
			      ttex[tedges] = t1;
			      tedges ++;
			    }
			}
		    }
		  
		  edges = tedges;
		  for(int i=0; i<edges; i++) poly[i] = tpoly[i];
		  for(int i=0; i<edges; i++) tex[i] = ttex[i];
		}
	    }
	  //---- clipping applied
	  
	  if (edges > 0)
	    {
	      glBegin(GL_POLYGON);
	      for(int i=0; i<edges; i++)
		{
		  glColor3f(tex[i].x, tex[i].y, tex[i].z);
		  glVertex3f(poly[i].x, poly[i].y, poly[i].z);
		}
	      glEnd();
	    }  
	}
    }
}


void
Viewer::volumeRaycast(float minZ, float maxZ, bool firstPartOnly)
{
  Vec eyepos = camera()->position();
  Vec viewDir = camera()->viewDirection();
  Vec subvolcorner = Vec(m_minHSlice, m_minWSlice, m_minDSlice);
  Vec subvolsize = Vec(m_maxHSlice-m_minHSlice+1,
		       m_maxWSlice-m_minWSlice+1,
		       m_maxDSlice-m_minDSlice+1);


  glBindFramebuffer(GL_FRAMEBUFFER_EXT, m_slcBuffer);
  for(int fbn=0; fbn<2; fbn++)
    {
      glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER,
			     GL_COLOR_ATTACHMENT0_EXT,
			     GL_TEXTURE_RECTANGLE_ARB,
			     m_slcTex[fbn],
			     0);
      glDrawBuffer(GL_COLOR_ATTACHMENT0_EXT);
      glClearColor(0, 0, 0, 0);
      glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }



  //----------------------------
  glBindFramebuffer(GL_FRAMEBUFFER_EXT, m_slcBuffer);
  glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER,
			 GL_COLOR_ATTACHMENT0_EXT,
			 GL_TEXTURE_RECTANGLE_ARB,
			 m_slcTex[1],
			 0);
  glDrawBuffer(GL_COLOR_ATTACHMENT0_EXT);  

  drawBox(GL_FRONT);

  glBindFramebuffer(GL_FRAMEBUFFER_EXT, 0);
  //----------------------------


  //----------------------------
  if (!m_fullRender || firstPartOnly)
    {
      glBindFramebuffer(GL_FRAMEBUFFER_EXT, m_slcBuffer);
      glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER,
			     GL_COLOR_ATTACHMENT0_EXT,
			     GL_TEXTURE_RECTANGLE_ARB,
			     m_slcTex[0],
			     0);
      glDrawBuffer(GL_COLOR_ATTACHMENT0_EXT);  
    }

  float stepsize = 0.7/qMax(m_vsize.x,qMax(m_vsize.y,m_vsize.z));
  if (m_dragMode && !(m_paintHit || m_carveHit)) stepsize *= 2;


  glUseProgramObjectARB(m_rcShader);
  glUniform1iARB(m_rcParm[0], 2); // dataTex
  glUniform1iARB(m_rcParm[1], 3); // lutTex
  glUniform1iARB(m_rcParm[2], 1); // slcTex - contains exit coordinates
  glUniform1fARB(m_rcParm[3], stepsize); // stepSize
  glUniform3fARB(m_rcParm[4], eyepos.x, eyepos.y, eyepos.z); // eyepos
  glUniform3fARB(m_rcParm[5], viewDir.x, viewDir.y, viewDir.z); // viewDir
  glUniform3fARB(m_rcParm[6], subvolcorner.x, subvolcorner.y, subvolcorner.z);
  glUniform3fARB(m_rcParm[7], subvolsize.x, subvolsize.y, subvolsize.z);
  glUniform1fARB(m_rcParm[8], minZ); // minZ
  glUniform1fARB(m_rcParm[9], maxZ); // maxZ
  glUniform1iARB(m_rcParm[10],4); // maskTex
  glUniform1iARB(m_rcParm[11],firstPartOnly); // save voxel coordinates
  glUniform1iARB(m_rcParm[12],m_skipLayers); // skip first layers
  glUniform1iARB(m_rcParm[13],5); // tagTex

  glActiveTexture(GL_TEXTURE1);
  glEnable(GL_TEXTURE_RECTANGLE_ARB);
  glBindTexture(GL_TEXTURE_RECTANGLE_ARB, m_slcTex[1]);

  glActiveTexture(GL_TEXTURE2);
  glEnable(GL_TEXTURE_3D);
  glBindTexture(GL_TEXTURE_3D, m_dataTex);

  glActiveTexture(GL_TEXTURE4);
  glEnable(GL_TEXTURE_3D);
  glBindTexture(GL_TEXTURE_3D, m_maskTex);

  uchar *lut = Global::lut();
  glActiveTexture(GL_TEXTURE3);
  glEnable(GL_TEXTURE_2D);
  glBindTexture(GL_TEXTURE_2D, m_lutTex);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); 
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE); 
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,GL_NEAREST);
  glTexImage2D(GL_TEXTURE_2D,
	       0, // single resolution
	       GL_RGBA,
	       256, Global::lutSize()*256, // width, height
	       0, // no border
	       GL_BGRA,
	       GL_UNSIGNED_BYTE,
	       lut);

  uchar *tagColors = Global::tagColors();
  glActiveTexture(GL_TEXTURE5);
  glEnable(GL_TEXTURE_1D);
  glBindTexture(GL_TEXTURE_1D, m_tagTex);
  glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); 
  glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER,GL_NEAREST);
  glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER,GL_NEAREST);
  glTexImage1D(GL_TEXTURE_1D,
	       0, // single resolution
	       GL_RGBA,
	       256,
	       0, // no border
	       GL_RGBA,
	       GL_UNSIGNED_BYTE,
	       tagColors);

  drawBox(GL_BACK);

  if (!m_fullRender || firstPartOnly)
    glBindFramebuffer(GL_FRAMEBUFFER_EXT, 0);
  //----------------------------

  glActiveTexture(GL_TEXTURE4);
  glDisable(GL_TEXTURE_3D);

  if (firstPartOnly)
    {
      glBindFramebuffer(GL_FRAMEBUFFER_EXT, 0);
      glUseProgramObjectARB(0);
      
      glActiveTexture(GL_TEXTURE5);
      glDisable(GL_TEXTURE_1D);

      glActiveTexture(GL_TEXTURE3);
      glDisable(GL_TEXTURE_2D);
      
      glActiveTexture(GL_TEXTURE2);
      glDisable(GL_TEXTURE_3D);
      
      glActiveTexture(GL_TEXTURE1);
      glDisable(GL_TEXTURE_RECTANGLE_ARB);
      return;
    }


  if (!m_fullRender)
    {
      glColor3f(0.9, 0.9, 1.0);
      //--------------------------------
      glUseProgramObjectARB(m_eeShader);
      
      glActiveTexture(GL_TEXTURE1);
      glEnable(GL_TEXTURE_RECTANGLE_ARB);
      glBindTexture(GL_TEXTURE_RECTANGLE_ARB, m_slcTex[0]);
      
      uchar *tagColors = Global::tagColors();
      glActiveTexture(GL_TEXTURE5);
      glEnable(GL_TEXTURE_1D);
      glBindTexture(GL_TEXTURE_1D, m_tagTex);
      glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); 
      glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER,GL_NEAREST);
      glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER,GL_NEAREST);
      glTexImage1D(GL_TEXTURE_1D,
		   0, // single resolution
		   GL_RGBA,
		   256,
		   0, // no border
		   GL_RGBA,
		   GL_UNSIGNED_BYTE,
		   tagColors);

      glUniform1iARB(m_eeParm[0], 1); // slctex
      glUniform1fARB(m_eeParm[1], minZ); // minZ
      glUniform1fARB(m_eeParm[2], maxZ); // maxZ
      glUniform3fARB(m_eeParm[3], eyepos.x, eyepos.y, eyepos.z); // eyepos
      glUniform3fARB(m_eeParm[4], viewDir.x, viewDir.y, viewDir.z); // viewDir
      glUniform1fARB(m_eeParm[5], m_dzScale); // dzScale
      glUniform1iARB(m_eeParm[6], 5); // tagtex
      
      int wd = camera()->screenWidth();
      int ht = camera()->screenHeight();
      StaticFunctions::pushOrthoView(0, 0, wd, ht);
      StaticFunctions::drawQuad(0, 0, wd, ht, 1);
      StaticFunctions::popOrthoView();
      //----------------------------
    }

  glUseProgramObjectARB(0);

  glActiveTexture(GL_TEXTURE5);
  glDisable(GL_TEXTURE_1D);

  glActiveTexture(GL_TEXTURE3);
  glDisable(GL_TEXTURE_2D);

  glActiveTexture(GL_TEXTURE2);
  glDisable(GL_TEXTURE_3D);

  glActiveTexture(GL_TEXTURE1);
  glDisable(GL_TEXTURE_RECTANGLE_ARB);
}

void
Viewer::uploadMask(int dst, int wst, int hst, int ded, int wed, int hed)
{
  int ds = m_minDSlice + m_sslevel*qFloor((dst-m_minDSlice)/m_sslevel);
  int ws = m_minWSlice + m_sslevel*qFloor((wst-m_minWSlice)/m_sslevel);
  int hs = m_minHSlice + m_sslevel*qFloor((hst-m_minHSlice)/m_sslevel);

  int de = m_minDSlice + m_sslevel*qCeil((ded-m_minDSlice)/m_sslevel);
  int we = m_minWSlice + m_sslevel*qCeil((wed-m_minWSlice)/m_sslevel);
  int he = m_minHSlice + m_sslevel*qCeil((hed-m_minHSlice)/m_sslevel);

  ds = qBound(m_minDSlice, ds, m_maxDSlice);
  ws = qBound(m_minWSlice, ws, m_maxWSlice);
  hs = qBound(m_minHSlice, hs, m_maxHSlice);

  de = qBound(m_minDSlice, de, m_maxDSlice);
  we = qBound(m_minWSlice, we, m_maxWSlice);
  he = qBound(m_minHSlice, he, m_maxHSlice);

  int dsz = (de-ds)/m_sslevel;
  int wsz = (we-ws)/m_sslevel;
  int hsz = (he-hs)/m_sslevel;
  if (dsz*m_sslevel < de-ds) dsz++;
  if (wsz*m_sslevel < we-ws) wsz++;
  if (hsz*m_sslevel < he-hs) hsz++;
  int tsz = dsz*wsz*hsz;      
  uchar *voxelVol = new uchar[tsz];
  int i = 0;
  for(int d=ds; d<de; d+=m_sslevel)
    for(int w=ws; w<we; w+=m_sslevel)
      for(int h=hs; h<he; h+=m_sslevel)
	{
	  voxelVol[i] = m_maskPtr[d*m_width*m_height + w*m_height + h];
	  i++;
	}

  int doff = (ds-m_minDSlice)/m_sslevel;
  int woff = (ws-m_minWSlice)/m_sslevel;
  int hoff = (hs-m_minHSlice)/m_sslevel;

  glActiveTexture(GL_TEXTURE4);
  glEnable(GL_TEXTURE_3D);
  glBindTexture(GL_TEXTURE_3D, m_maskTex);	 
  glTexSubImage3D(GL_TEXTURE_3D,
		  0, // level
		  hoff, woff, doff, // offset
		  hsz, wsz, dsz,
		  GL_LUMINANCE,
		  GL_UNSIGNED_BYTE,
		  voxelVol);
  glDisable(GL_TEXTURE_3D);

  delete [] voxelVol;
}
