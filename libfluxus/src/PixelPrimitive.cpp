// Copyright (C) 2005 Dave Griffiths
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

#include <GL/glew.h>
#include "Renderer.h"
#include "PixelPrimitive.h"
#include "State.h"
#include "Utils.h"

//#define RENDER_NORMALS
//#define RENDER_BBOX
//#define DEBUG_GL

using namespace Fluxus;

#ifdef DEBUG_GL
static void check_gl_errors(const char *call)
{
	GLenum status = glGetError();
	if (status == GL_NO_ERROR)
		return;

	const char *status_msg = (const char *)gluErrorString(status);
	if (status_msg == NULL)
		status_msg = "unknown gl error";

	cerr << call << " - " << status_msg << " (" << status << ")" << endl;
}

static void check_fbo_errors(void)
{
	const char *fbo_status_msg[] =
	{
	  "GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT_EXT",
	  "GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT_EXT",
	  "unknown GL_FRAMEBUFFER error",
	  "GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS_EXT",
	  "GL_FRAMEBUFFER_INCOMPLETE_FORMATS_EXT",
	  "GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER_EXT",
	  "GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER_EXT",
	  "GL_FRAMEBUFFER_UNSUPPORTED_EXT",
	  "unknown GL_FRAMEBUFFER error"
	};
	const unsigned fbo_status_msg_count =
		sizeof(fbo_status_msg)/sizeof(fbo_status_msg[0]);

	GLenum status = glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT);
	if (status == GL_FRAMEBUFFER_COMPLETE_EXT)
		return;

	status -= GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT_EXT;
	if (status >= fbo_status_msg_count)
		status = fbo_status_msg_count-1;
	cerr << "fbo - " << fbo_status_msg[status] << endl;
}
#endif


PixelPrimitive::PixelPrimitive(unsigned int w, unsigned int h, bool RendererActive /* = false */) :
m_Texture(0),
m_DepthBuffer(0),
m_FBO(0),
m_Width(w),
m_Height(h),
m_ReadyForUpload(false),
m_ReadyForDownload(false),
m_RendererActive(RendererActive)
{
	m_FBOSupported = glewIsSupported("GL_EXT_framebuffer_object");
	m_Renderer = new Renderer();
	m_Physics = new Physics(m_Renderer);

	AddData("c",new TypedPData<dColour>);

	// setup the direct access for speed
	PDataDirty();

	for (unsigned int x=0; x<h; x++)
	{
		for (unsigned int y=0; y<w; y++)
		{
			m_ColourData->push_back(dColour(1,1,1));
		}
	}

	m_Points.push_back(dVector(0,0,0));
	m_Points.push_back(dVector(1,0,0));
	m_Points.push_back(dVector(1,1,0));
	m_Points.push_back(dVector(0,1,0));

	if (m_FBOSupported)
	{
		ResizeFBO(m_Width, m_Height);
	}
	else
	{
		glGenTextures(1, (GLuint*)&m_Texture);

		glBindTexture(GL_TEXTURE_2D, m_Texture);
		gluBuild2DMipmaps(GL_TEXTURE_2D, 4, m_Width, m_Height,
				GL_RGBA, GL_FLOAT, &(*m_ColourData)[0]);
		glBindTexture(GL_TEXTURE_2D, 0);

		cerr << "FBO is not supported" << endl;
	}
}

PixelPrimitive::PixelPrimitive(const PixelPrimitive &other) :
Primitive(other),
m_Points(other.m_Points),
m_Width(other.m_Width),
m_Height(other.m_Height),
m_ReadyForUpload(other.m_ReadyForUpload),
m_ReadyForDownload(other.m_ReadyForDownload),
m_FBOSupported(other.m_FBOSupported),
m_RendererActive(other.m_RendererActive)
{
	m_Renderer = new Renderer();
	m_Physics = new Physics(m_Renderer);

	PDataDirty();
	if (m_FBOSupported)
	{
		ResizeFBO(m_Width, m_Height);
	}
	else
	{
		glGenTextures(1, (GLuint*)&m_Texture);
	}
}

PixelPrimitive::~PixelPrimitive()
{
	if (m_Texture!=0)
	{
		glDeleteTextures(1,(GLuint*)&m_Texture);
	}

	if (m_FBOSupported)
	{
		glDeleteFramebuffersEXT(1, (GLuint *)&m_FBO);
		if (m_DepthBuffer!=0) glDeleteRenderbuffersEXT(1, (GLuint *)&m_DepthBuffer);
	}

	/* FIXME: segfaults */
	//delete m_Renderer;
}

PixelPrimitive* PixelPrimitive::Clone() const
{
	return new PixelPrimitive(*this);
}

void PixelPrimitive::PDataDirty()
{
	// reset pointers
	m_ColourData=GetDataVec<dColour>("c");
}

void PixelPrimitive::ResizeFBO(int w, int h)
{
	if (m_FBOSupported)
	{
		if (m_Texture != 0)
			glDeleteTextures(1,(GLuint*)&m_Texture);
		if (m_FBO != 0)
			glDeleteFramebuffersEXT(1, (GLuint *)&m_FBO);
		if (m_DepthBuffer!=0)
			glDeleteRenderbuffersEXT(1, (GLuint *)&m_DepthBuffer);

		glGenTextures(1, (GLuint*)&m_Texture);

		m_FBOWidth = 1 << (unsigned)ceil(log2(w));
		m_FBOHeight = 1 << (unsigned)ceil(log2(h));

		m_Renderer->SetResolution(m_Width, m_Height);

		/* setup the framebuffer */
		glGenFramebuffersEXT(1, (GLuint *)&m_FBO);
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, (GLuint)m_FBO);

		glBindTexture(GL_TEXTURE_2D, m_Texture);

		/* set texture parameters */
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
				m_State.TextureStates[0].Mag);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
				m_State.TextureStates[0].Min);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,
				m_State.TextureStates[0].WrapS);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,
				m_State.TextureStates[0].WrapT);
		glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_TRUE);

		/* create a texture of m_FBOWidth x m_FBOHeight size */
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, m_FBOWidth, m_FBOHeight, 0,
				GL_RGBA, GL_FLOAT, NULL);

		/* establish a mipmap chain for the texture */
		glGenerateMipmapEXT(GL_TEXTURE_2D);
#ifdef DEBUG_GL
		check_gl_errors("ResizeFBO glGenerateMipmapEXT");
#endif

		/* upload pdata to the top left corner of m_Width x m_Height size */
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_Width, m_Height,
				GL_RGBA, GL_FLOAT, &(*m_ColourData)[0]);
#ifdef DEBUG_GL
		check_gl_errors("ResizeFBO glTexSubImage2D");
#endif

		/* attach the texture to the FBO as GL_COLOR_ATTACHMENT0_EXT */
		glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT,
				GL_COLOR_ATTACHMENT0_EXT,
				GL_TEXTURE_2D, (GLuint)m_Texture, 0);

		/* create the depth buffer */
		if (m_State.Hints & HINT_IGNORE_DEPTH)
		{
			m_DepthBuffer = 0;
			//cout << " no depth" << endl;
		}
		else
		{
			glGenRenderbuffersEXT(1, (GLuint *)&m_DepthBuffer);
			glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, (GLuint)m_DepthBuffer);
			glRenderbufferStorageEXT(GL_RENDERBUFFER_EXT, GL_DEPTH_COMPONENT24,
					m_FBOWidth, m_FBOHeight);
		}

		/* attach the texture to the FBO as GL_COLOR_ATTACHMENT0_EXT */
		glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT,
				GL_COLOR_ATTACHMENT0_EXT,
				GL_TEXTURE_2D, (GLuint)m_Texture, 0);

		/* attach the depth buffer to the fbo */
		if (m_DepthBuffer!=0)
			glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT,
					GL_DEPTH_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT,
					(GLuint)m_DepthBuffer);
#ifdef DEBUG_GL
		check_fbo_errors();
#endif

		/* unbind the fbo */
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
		glBindTexture(GL_TEXTURE_2D, 0);

		m_FBOMaxS = (float)w / (float)m_FBOWidth;
		m_FBOMaxT = (float)h / (float)m_FBOHeight;

#ifdef DEBUG_GL
		cout << "pix created " << dec << m_FBOWidth << "x" << m_FBOHeight << " (" << m_Width <<
			"x" << m_Height << ") " << hex << this << endl;
		cout << "\trenderer: " << hex << m_Renderer << endl;
#endif
	}
}

void PixelPrimitive::Upload()
{
	m_ReadyForUpload = true;
}

void PixelPrimitive::Download()
{
	m_ReadyForDownload = true;
}

void PixelPrimitive::Load(const string &filename)
{
	TypedPData<dColour> *data = dynamic_cast<TypedPData<dColour>*>(GetDataRaw("c"));
	if (data)
	{
		unsigned ow = m_Width;
		unsigned oh = m_Height;

		TexturePainter::Get()->LoadPData(filename,m_Width,m_Height,*data);

		if ((ow != m_Width) || (oh != m_Height))
		{
			ResizeFBO(m_Width, m_Height);
		}
	}
}

void PixelPrimitive::Save(const string &filename) const
{
	const TypedPData<dColour> *data = dynamic_cast<const TypedPData<dColour>*>(GetDataRawConst("c"));
	if (data)
	{
		TexturePainter::Get()->SavePData(filename,m_Width,m_Height,*data);
	}
}

void PixelPrimitive::Bind()
{
	if (!m_FBOSupported)
		return;

	glPushAttrib(GL_ALL_ATTRIB_BITS);

	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_FBO);

	/* set rendering */
	glDrawBuffer(GL_COLOR_ATTACHMENT0_EXT);
}

void PixelPrimitive::Unbind()
{
	if (!m_FBOSupported)
		return;

	glPopAttrib();
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);

	// generate mipmaps
	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, m_Texture);
	glGenerateMipmapEXT(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, 0);
	glDisable(GL_TEXTURE_2D);
	//cout << "pix " << "unbound " << hex << this << endl;
}

void PixelPrimitive::Render()
{
	// we need to do uploading while we have an active gl context
	if (m_ReadyForUpload)
	{
		UploadPData();
		m_ReadyForUpload=false;
	}

	// render the pixel primitive scenegraph
	if (m_FBOSupported && m_RendererActive)
	{
		glPushMatrix();
		Bind();
		m_Renderer->Reinitialise();
		m_Renderer->Render();
		Unbind();
		glPopMatrix();
	}

	if (m_State.Hints & HINT_WIRE)
	{
		glDisable(GL_TEXTURE_2D);
		glDisable(GL_LIGHTING);
		glPolygonOffset(1, 1);
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
		glColor4fv(m_State.WireColour.arr());

		glBegin(GL_QUADS);
		glVertex3fv(m_Points[0].arr());
		glVertex3fv(m_Points[1].arr());
		glVertex3fv(m_Points[2].arr());
		glVertex3fv(m_Points[3].arr());
		glEnd();

		glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
		glEnable(GL_LIGHTING);
		glEnable(GL_TEXTURE_2D);
	}

	float s = m_FBOSupported ? m_FBOMaxS : 1;
	float t = m_FBOSupported ? m_FBOMaxT : 1;

	// override the state texture!
	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D,m_Texture);

	glDisable(GL_LIGHTING);
	glBegin(GL_QUADS);
	glTexCoord2f(0,0);
	glVertex3fv(m_Points[0].arr());
	glTexCoord2f(s,0);
	glVertex3fv(m_Points[1].arr());
	glTexCoord2f(s,t);
	glVertex3fv(m_Points[2].arr());
	glTexCoord2f(0,t);
	glVertex3fv(m_Points[3].arr());
	glEnd();
	glEnable(GL_LIGHTING);
	glDisable(GL_TEXTURE_2D);

	if (m_ReadyForDownload)
	{
		DownloadPData();
		m_ReadyForDownload=false;
	}
}

dBoundingBox PixelPrimitive::GetBoundingBox(const dMatrix &space)
{
	dBoundingBox box;
	for (vector<dVector>::iterator i=m_Points.begin(); i!=m_Points.end(); ++i)
	{
		box.expand(space.transform(*i));
	}
	return box;
}

void PixelPrimitive::ApplyTransform(bool ScaleRotOnly)
{
	if (!ScaleRotOnly)
	{
		for (vector<dVector>::iterator i=m_Points.begin(); i!=m_Points.end(); ++i)
		{
			*i=GetState()->Transform.transform(*i);
		}
	}
	else
	{
		for (unsigned int i=0; i<m_Points.size(); i++)
		{
			m_Points[i]=GetState()->Transform.transform_no_trans(m_Points[i]);
		}
	}

	GetState()->Transform.init();
}

void PixelPrimitive::UploadPData()
{
	if (m_FBOSupported)
	{
		glBindTexture(GL_TEXTURE_2D, m_Texture);
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_Width, m_Height,
				GL_RGBA, GL_FLOAT, &(*m_ColourData)[0]);
		glBindTexture(GL_TEXTURE_2D, 0);
	}
	else
	{
		glBindTexture(GL_TEXTURE_2D, m_Texture);
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_Width, m_Height,
				GL_RGBA, GL_FLOAT, &(*m_ColourData)[0]);
	}
}

void PixelPrimitive::DownloadPData()
{
	if (m_FBOSupported)
	{
		Bind();
		GLubyte *data=GetScreenBuffer(0, 0, m_Width, m_Height, 1);
		for (unsigned int i=0; i<m_Width*m_Height; i++)
		{
			(*m_ColourData)[i].r=data[i*3]/255.0f;
			(*m_ColourData)[i].g=data[i*3+1]/255.0f;
			(*m_ColourData)[i].b=data[i*3+2]/255.0f;
		}
		Unbind();
	}
}

