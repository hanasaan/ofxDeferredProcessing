#include "Processor.h"

using namespace DeferredEffect;

void RenderPass::texturedQuad(float x, float y, float width, float height, float s, float t)
{
    // TODO: change to triangle fan/strip
    glBegin(GL_QUADS);
    glTexCoord2f(0, 0);
    glVertex3f(x, y, 0);
    
    glTexCoord2f(s, 0);
    glVertex3f(x + width, y, 0);
    
    glTexCoord2f(s, t);
    glVertex3f(x + width, y + height, 0);
    
    glTexCoord2f(0, t);
    glVertex3f(x, y + height, 0);
    glEnd();
}

void Processor::init(unsigned width, unsigned height)
{
    this->width = width;
    this->height = height;
    
    
    // no need to use depth for ping pongs
    for (int i = 0; i < 2; ++i)
    {
        pingPong[i].allocate(width, height, GL_RGBA);
    }
    
    ofFbo::Settings s;
    s.width = width;
    s.height = height;
    s.textureTarget = GL_TEXTURE_RECTANGLE_ARB;
    s.useDepth = true;
    s.useStencil = true;
    s.depthStencilAsTexture = true;
    raw.allocate(s);
    
    numProcessedPasses = 0;
    currentReadFbo = 0;
    
    gbuffer.setup(width, height);
}

void Processor::begin(ofCamera& cam)
{
    // update camera matrices
    cam.begin();
    cam.end();
    
    for (int i = 0; i < passes.size(); ++i) {
        if (passes[i]->getEnabled()) {
            passes[i]->update(cam);
        }
    }
    
    raw.begin();
    
    ofPushView();
    
    ofRectangle viewport(0, 0, raw.getWidth(), raw.getHeight());
    ofViewport(viewport);
    ofSetOrientation(ofGetOrientation(),cam.isVFlipped());
    ofSetMatrixMode(OF_MATRIX_PROJECTION);
    ofLoadMatrix(cam.getProjectionMatrix(viewport));
    ofSetMatrixMode(OF_MATRIX_MODELVIEW);
    ofLoadMatrix(cam.getModelViewMatrix());
    
    ofPushStyle();
    glPushAttrib(GL_ENABLE_BIT);
}

void Processor::end(bool autoDraw)
{
    glPopAttrib();
    ofPopStyle();
    ofPopView();
    
    raw.end();
    
    ofPushStyle();
    glPushAttrib(GL_ENABLE_BIT);
    glDisable(GL_LIGHTING);
    ofSetColor(255, 255, 255);
    process();
    if (autoDraw) draw();
    glPopAttrib();
    ofPopStyle();
}

void Processor::debugDraw()
{
    raw.getTextureReference().draw(10, 10, 300, 300);
    raw.getDepthTexture().draw(320, 10, 300, 300);
    pingPong[currentReadFbo].draw(630, 10, 300, 300);
}

void Processor::draw(float x, float y) const
{
    draw(x, y, width, height);
}

void Processor::draw(float x, float y, float w, float h) const
{
    if (numProcessedPasses == 0) raw.draw(0, 0, w, h);
    else pingPong[currentReadFbo].draw(0, 0, w, h);
}

ofTexture& Processor::getProcessedTextureReference()
{
    if (numProcessedPasses) return pingPong[currentReadFbo].getTextureReference();
    else return raw.getTextureReference();
}

// need to have depth enabled for some fx
void Processor::process(ofFbo& raw)
{
    numProcessedPasses = 0;
    for (int i = 0; i < passes.size(); ++i)
    {
        if (passes[i]->getEnabled())
        {
            if (numProcessedPasses == 0) passes[i]->render(raw, pingPong[1 - currentReadFbo], gbuffer);
            else passes[i]->render(pingPong[currentReadFbo], pingPong[1 - currentReadFbo], gbuffer);
            currentReadFbo = 1 - currentReadFbo;
            numProcessedPasses++;
        }
    }
}

void Processor::process()
{
    process(raw);
}
