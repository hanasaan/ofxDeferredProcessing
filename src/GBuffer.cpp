//
// Created by Yuya Hanai, https://github.com/hanasaan
//

#include "GBuffer.h"

using namespace DeferredEffect;

#define STRINGIFY(A) #A

// Note : This class is thread unsafe.
string gbufferVertShader = STRINGIFY
(
 uniform mat4 invCurrentMvpMat;
 uniform mat4 prevMvpMat;
 uniform mat4 invCurrentTransformMat;
 uniform mat4 prevTransformMat;
 uniform float farClip;
 varying float v_depth;
 varying vec3 v_normal;
 varying vec2 v_texCoord;
 varying vec2 v_velocity;
 
 void main()
 {
     mat4 postTransformMatrix = invCurrentTransformMat * invCurrentMvpMat * gl_ModelViewProjectionMatrix;
     gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;
     vec4 currentPosition = gl_Position;
     vec4 prevPosition = prevMvpMat * prevTransformMat * postTransformMatrix * gl_Vertex;
     currentPosition.xyz = currentPosition.xyz / currentPosition.w;
     prevPosition.xyz = prevPosition.xyz / prevPosition.w;
     
     // half spread velocity
     vec2 velocity = (currentPosition.xy - prevPosition.xy) * 0.5;
     velocity.y = -velocity.y;
     
     // velocity encoding :
     // http://www.crytek.com/download/Sousa_Graphics_Gems_CryENGINE3.pdf
     velocity = sign(velocity) * sqrt(abs(velocity)) * 127.0 / 255.0 + vec2(127.0/255.0);
     vec4 viewPos = gl_ModelViewMatrix * gl_Vertex;
     v_depth = -viewPos.z / farClip;
     v_normal = gl_NormalMatrix * gl_Normal;
     v_texCoord = gl_MultiTexCoord0.st;
     v_velocity = velocity;
     
     gl_FrontColor = gl_Color;
 }
 );

string gbufferFragShader = STRINGIFY
(
 uniform sampler2DRect tex;
 uniform float texFlag;
 varying vec3 v_normal;
 varying vec2 v_texCoord;
 varying float v_depth;
 varying vec2 v_velocity;
 
 void main()
 {
     vec3 diffuse = texture2DRect(tex, v_texCoord.st).rgb;
     gl_FragData[0] = mix(gl_Color, gl_Color * vec4(diffuse, 1.0), texFlag); // albedo
     gl_FragData[1] = vec4(normalize(v_normal),  v_depth); // normal + depth
     gl_FragData[2] = vec4(v_velocity.x, v_velocity.y, 0.0, 1.0); // velocity
 }
 );


string alphaFragShader = STRINGIFY
(
 uniform sampler2DRect tex;
 void main()
 {
     gl_FragColor = vec4(vec3(texture2DRect(tex, gl_TexCoord[0].xy).a), 1.0);
 }
);

//======================================================================================
void GBufferObject::flush() {
    prevGlobalTransformMatrix = getGlobalTransformMatrix();
}

void GBufferObject::drawToGBuffer(bool autoFlush) {
    if (currentShader) {
        currentShader->setUniformMatrix4f("prevTransformMat", prevGlobalTransformMatrix);
        currentShader->setUniformMatrix4f("invCurrentTransformMat", getGlobalTransformMatrix().getInverse());
    }
    draw();
    if (currentShader) {
        currentShader->setUniformMatrix4f("prevTransformMat", ofMatrix4x4());
        currentShader->setUniformMatrix4f("invCurrentTransformMat", ofMatrix4x4());
    }
    if (autoFlush) {
        flush();
    }
}

//======================================================================================
void GBuffer::setup(int w, int h)
{
    ofFbo::Settings settings;
    settings.width = w;
    settings.height = h;
    settings.minFilter = GL_NEAREST;
    settings.maxFilter = GL_NEAREST;
    settings.colorFormats.push_back(GL_RGB);           // albedo
    settings.colorFormats.push_back(GL_RGBA32F_ARB);   // normal + ldepth
    settings.colorFormats.push_back(GL_RG8);           // velocity
    settings.colorFormats.push_back(GL_RGB);           // light pass
    settings.depthStencilAsTexture = true;
    settings.useDepth = true;
    settings.useStencil = true;
    fbo.allocate(settings);
    
    shader.setupShaderFromSource(GL_VERTEX_SHADER, gbufferVertShader);
    shader.setupShaderFromSource(GL_FRAGMENT_SHADER, gbufferFragShader);
    shader.linkProgram();
    
    debugShader.setupShaderFromSource(GL_FRAGMENT_SHADER, alphaFragShader);
    debugShader.linkProgram();
}

void GBuffer::begin(ofCamera& cam, Mode mode)
{
    // update camera first
    cam.begin();
    cam.end();
    
    currentShader = &shader;
    fbo.begin();
    
    if (mode == MODE_GEOMETRY) {
        vector<int> bufferInt;
        bufferInt.push_back(TYPE_ALBEDO);
        bufferInt.push_back(TYPE_NORMAL_DEPTH);
        bufferInt.push_back(TYPE_VELOCITY);
        fbo.setActiveDrawBuffers(bufferInt);
    } else if (mode == MODE_LIGHT) {
        fbo.setActiveDrawBuffer(TYPE_LIGHT_PASS);
    }
    ofClear(128, 128, 128, 255);
    ofPushView();
    
    ofRectangle viewport(0, 0, fbo.getWidth(), fbo.getHeight());
    ofViewport(viewport);
    ofSetOrientation(ofGetOrientation(),cam.isVFlipped());
    ofSetMatrixMode(OF_MATRIX_PROJECTION);
    ofLoadMatrix(cam.getProjectionMatrix(viewport));
    ofSetMatrixMode(OF_MATRIX_MODELVIEW);
    ofLoadMatrix(cam.getModelViewMatrix());
    shader.begin();
    shader.setUniform1f("farClip", cam.getFarClip());
    shader.setUniformMatrix4f("prevMvpMat", prevModelviewProjectionMatrix);
    shader.setUniformMatrix4f("invCurrentMvpMat", cam.getModelViewProjectionMatrix().getInverse());
    prevModelviewProjectionMatrix = cam.getModelViewProjectionMatrix();
    
    ofPushStyle();
    ofEnableDepthTest();
    ofDisableAlphaBlending();
}

void GBuffer::end()
{
    ofDisableDepthTest();
    ofPopStyle();
    
    shader.end();
    
    ofPopView();
    fbo.end();
    currentShader = NULL;
}

void GBuffer::debugDraw()
{
    ofDisableAlphaBlending();
    float w2 = ofGetViewportWidth();
    float h2 = ofGetViewportHeight();
    float ws = w2*0.25;
    float hs = h2*0.25;
    
    fbo.getTextureReference(0).draw(0, hs*3, ws, hs);
    fbo.getTextureReference(1).draw(ws, hs*3, ws, hs);
    fbo.getTextureReference(2).draw(ws*2, hs*3, ws, hs);
    
    debugShader.begin();
    fbo.getTextureReference(1).draw(ws*3, hs*3, ws, hs);
    debugShader.end();
}
