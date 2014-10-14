#pragma once

#include "ofMain.h"

#define STRINGIFY(A) #A

// This is based on the paper "A Reconstruction Filter for Plausible Motion Blur"
// (http://graphics.cs.williams.edu/papers/MotionBlurI3D12/)
string velocityVertShader = STRINGIFY
(
 uniform mat4 invCurrentMvpMat;
 uniform mat4 prevMvpMat;
 uniform float fps;
 uniform float exposureTime;
 uniform float farClip;
 uniform int depthMask;
 varying float depth;
 
 void main()
 {
     mat4 postTransformMatrix = invCurrentMvpMat * gl_ModelViewProjectionMatrix;
     gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;
     vec4 currentPosition = gl_Position;
     vec4 prevPosition = prevMvpMat * postTransformMatrix * gl_Vertex;
     currentPosition.xyz = currentPosition.xyz / currentPosition.w;
     prevPosition.xyz = prevPosition.xyz / prevPosition.w;
     
     // half spread velocity
     vec2 velocity = (currentPosition.xy - prevPosition.xy) * fps * exposureTime * 0.5;
     velocity.y = -velocity.y;
     
     // velocity encoding :
     // http://www.crytek.com/download/Sousa_Graphics_Gems_CryENGINE3.pdf
     velocity = sign(velocity) * sqrt(abs(velocity)) * 127.0 / 255.0 + vec2(127.0/255.0);
     gl_FrontColor = vec4(velocity.x, velocity.y, 0.0, 1.0);
     
     vec4 viewPos = gl_ModelViewMatrix * gl_Vertex;
     depth = -viewPos.z / farClip;
     if (depthMask == 0) {
         depth = 1.0;
     }
 }
 );

string velocityFragShader = STRINGIFY
(
 varying float depth;
 void main()
 {
     gl_FragData[0] = gl_Color;
     gl_FragData[1] = vec4(depth);
 }
 );

string tileMaxFragShader = STRINGIFY
(
 uniform sampler2DRect tex;  // This is reference size texture
 uniform sampler2DRect texVelocity; // This is velocity texture
 uniform float k;
 void main() {
     int startx = int(gl_TexCoord[0].x) * int(k);
     int starty = int(gl_TexCoord[0].y) * int(k);
     int endx = startx + int(k) - 1;
     int endy = starty + int(k) - 1;
     float velocitymax = 0.0;
     vec2 velocitymaxvec = vec2(127.0/255.0);
     for (int y=starty; y<=endy; ++y) {
         for (int x=startx; x<=endx; ++x) {
             vec2 vvec = texelFetch2DRect(texVelocity, ivec2(x,y)).xy;
             vec2 vvecdec = (vvec - vec2(127.0/255.0));
             float v = length(vvecdec);
             if (v > velocitymax) {
                 velocitymax = v;
                 velocitymaxvec = vvec;
             }
         }
     }
     gl_FragColor = vec4(velocitymaxvec.x, velocitymaxvec.y, 0.0, 1.0);
 }
 );

string neighborMaxFragShader = STRINGIFY
(
 uniform sampler2DRect tex;
 void main() {
     int u = int(gl_TexCoord[0].x);
     int v = int(gl_TexCoord[0].y);
     float velocitymax = 0.0;
     vec2 velocitymaxvec = vec2(127.0/255.0);
     for (int y=v-1; y<=v+1; ++y) {
         for (int x=u-1; x<=u+1; ++x) {
             vec2 vvec = texture2DRect(tex, vec2(x,y)).xy;
             vec2 vvecdec = (vvec - vec2(127.0/255.0));
             float v = length(vvecdec);
             if (v > velocitymax) {
                 velocitymax = v;
                 velocitymaxvec = vvec;
             }
         }
     }
     gl_FragColor = vec4(velocitymaxvec.x, velocitymaxvec.y, 0.0, 1.0);
 }
 );

string reconstructionFragShader = STRINGIFY
(
 uniform sampler2DRect tex;
 uniform sampler2DRect texVelocity;
 uniform sampler2DRect linearDepth;
 uniform sampler2DRect neighborMax;
 uniform float k;
 uniform float farClip;
 uniform int S;
 uniform vec2 viewport;
 
 const float SOFT_Z_EXTENT = 1.0;
 
 // convert to pixel space
 vec2 decodeVelocity(const in vec2 v) {
     vec2 vd = v;
     vd = (vd - vec2(127.0/255.0));
     vd = (vd * vd) * sign(vd) * viewport;
     
     // clamp
     float length = clamp(length(vd), 0.0, k);
     vd = normalize(vd) * length;
     
     return vd;
 }
 
 float rand(vec2 co) {
     return fract(sin(dot(co.xy ,vec2(12.9898,78.233))) * 43758.5453);
 }
 
 float softDepthCompare(float za, float zb) {
     return clamp(1.0 - (za - zb)/SOFT_Z_EXTENT, 0.0, 1.0);
 }
 
 float cone(vec2 x, vec2 y, vec2 v) {
     return clamp(1.0 - length(x-y)/length(v), 0.0, 1.0);
 }
  
 float cylinder(vec2 x, vec2 y, vec2 v) {
     return 1.0 - smoothstep(0.95*length(v), 1.05*length(v), length(x-y));
 }
 
 void main() {
     vec2 uv = gl_TexCoord[0].xy;
     int x = int(uv.x / k);
     int y = int(uv.y / k);
     vec2 vmax = decodeVelocity(texelFetch2DRect(neighborMax, ivec2(x,y)).xy);
     if (length(vmax) < 0.5) {
         gl_FragColor = texture2DRect(tex, uv);
         return;
     }
     vec2 v = decodeVelocity(texelFetch2DRect(texVelocity, ivec2(uv)).xy);
     float lv = clamp(length(v), 1.0, k);
     float weight = 1.0 / lv;
     vec4 sum = texture2DRect(tex, uv) * weight;
     float j = -0.5 + 1.0 * rand(uv);
     vec2 X = uv;
     for (int i=0; i<S; ++i) {
         if (i==((S-1)/2)) {
             continue;
         }
         float t = mix(-1.0, 1.0, (i + j + 1.0) / (S + 1.0));
         vec2 Y = floor(uv + vmax * t + vec2(0.5));
         float zx = farClip * texelFetch2DRect(linearDepth, ivec2(uv)).x;
         float zy = farClip * texelFetch2DRect(linearDepth, ivec2(Y)).x;
         vec2 vy = decodeVelocity(texelFetch2DRect(texVelocity, ivec2(Y)).xy);
         
         float f = softDepthCompare(zx, zy);
         float b = softDepthCompare(zy, zx);
         float ay = b * cone(Y, X, vy) + f * cone(X, Y, v) + cylinder(Y, X, vy) * cylinder(X, Y, v) * 2.0;
         weight += ay;
         sum += texture2DRect(tex, Y) * ay;
     }
     gl_FragColor = sum / weight;
 }
 );

class MotionBlurNode : public ofNode {
    ofMatrix4x4 prevGlobalTransformMatrix;
public:
    void flush() {
        prevGlobalTransformMatrix = getGlobalTransformMatrix();
    }
    const ofMatrix4x4& getPrevGlobalTransformMatrix() const {
        return prevGlobalTransformMatrix;
    }
protected:
    virtual void customDraw() = 0;
};

//========================================================================
class MotionBlur {
private:
    class SphereMotionBlurNode : public MotionBlurNode {
    public:
        SphereMotionBlurNode() {}
        
    protected:
        void customDraw() {
            ofPushStyle();
            ofSetSphereResolution(24);
            ofDrawSphere(ofVec3f(), 300);
            ofPopStyle();
        }
    } sphereNode;
    
public:
    struct Settings {
        float exposureTime;
        int S;
        Settings() {
            exposureTime = 0.030f;
            S = 15;
        }
    } settings;
    
private:
    ofFbo fbo;
    ofFbo fboTileMax;
    ofFbo fboNeighborMax;
    ofFbo fboRead;
    ofFbo fboWrite;
    ofShader velocityShader;
    ofShader tileMaxShader;
    ofShader neighborMaxShader;
    ofShader reconstructionShader;
    vector<MotionBlurNode*> nodes;
    
    ofMatrix4x4 modelviewProjectionMatrix;
    ofMatrix4x4 prevModelviewProjectionMatrix;
    float k;
    int w, h;
    ofSpherePrimitive sphere;
    float farClip;
    
public:
    MotionBlur() : w(0), h(0), farClip(1000) {}
    
    void addMotionBlurNode(MotionBlurNode* node)
    {
        nodes.push_back(node);
    }
    
    void clear()
    {
        nodes.clear();
    }
    
    void init(unsigned width = ofGetWidth(), unsigned height = ofGetHeight(), float k = 20)
    {
        this->k = k;
        w = width;
        h = height;
        fboRead.allocate(width, height, GL_RGBA);
        fboWrite.allocate(width, height, GL_RGBA);
        fbo.allocate(width, height, GL_RG8);
        fbo.createAndAttachTexture(GL_R32F, 1);
        
        fboTileMax.allocate(w / k, h / k, GL_RG8);
        fboNeighborMax.allocate(w / k, h / k, GL_RG8);
        
        velocityShader.setupShaderFromSource(GL_VERTEX_SHADER, velocityVertShader);
        velocityShader.setupShaderFromSource(GL_FRAGMENT_SHADER, velocityFragShader);
        velocityShader.linkProgram();
        
        fboTileMax.getTextureReference().setTextureMinMagFilter(GL_NEAREST, GL_NEAREST);
        fboNeighborMax.getTextureReference().setTextureMinMagFilter(GL_NEAREST, GL_NEAREST);
        {
            stringstream ss;
            ss << "#version 120" << endl;
            ss << "#extension GL_EXT_gpu_shader4 : enable" << endl;
            ss << tileMaxFragShader;
            tileMaxShader.setupShaderFromSource(GL_FRAGMENT_SHADER, ss.str());
            tileMaxShader.linkProgram();
        }
        {
            stringstream ss;
            ss << "#version 120" << endl;
            ss << "#extension GL_EXT_gpu_shader4 : enable" << endl;
            ss << neighborMaxFragShader;
            neighborMaxShader.setupShaderFromSource(GL_FRAGMENT_SHADER, ss.str());
            neighborMaxShader.linkProgram();
        }
        {
            stringstream ss;
            ss << "#version 120" << endl;
            ss << "#extension GL_EXT_gpu_shader4 : enable" << endl;
            ss << reconstructionFragShader;
            reconstructionShader.setupShaderFromSource(GL_FRAGMENT_SHADER, ss.str());
            reconstructionShader.linkProgram();
        }
    }
    
    void renderPreparePass(ofCamera& cam)
    {
        fbo.begin();
        fbo.activateAllDrawBuffers();
        ofClear(0);
        cam.begin();
        ofMatrix4x4 modelviewProjectionMatrix = cam.getModelViewProjectionMatrix(ofRectangle(0, 0, w, h));
        sphereNode.setGlobalPosition(cam.getGlobalPosition());
        
        velocityShader.begin();
        velocityShader.setUniform1f("farClip", cam.getFarClip());
        velocityShader.setUniform1f("fps", ofGetFrameRate());
        velocityShader.setUniform1f("exposureTime", settings.exposureTime);
        
        // for camera movement blur
        ofDisableDepthTest();
        {
            velocityShader.setUniformMatrix4f("prevMvpMat", sphereNode.getPrevGlobalTransformMatrix() * prevModelviewProjectionMatrix);
            velocityShader.setUniformMatrix4f("invCurrentMvpMat", (sphereNode.getGlobalTransformMatrix() * modelviewProjectionMatrix).getInverse());
            velocityShader.setUniform1i("depthMask", 0);
            sphereNode.draw();
        }
        
        // for motion blur node blur
        ofEnableDepthTest();
        velocityShader.setUniform1i("depthMask", 1);
        for (MotionBlurNode* node : nodes) {
            velocityShader.setUniformMatrix4f("prevMvpMat", node->getPrevGlobalTransformMatrix() * prevModelviewProjectionMatrix);
            velocityShader.setUniformMatrix4f("invCurrentMvpMat", (node->getGlobalTransformMatrix() * modelviewProjectionMatrix).getInverse());
            node->draw();
        }
        velocityShader.end();
        cam.end();
        ofDisableDepthTest();
        fbo.end();
        
        // Tile Max
        fboTileMax.begin();
        tileMaxShader.begin();
        tileMaxShader.setUniformTexture("texVelocity", fbo.getTextureReference(), 1);
        tileMaxShader.setUniform1f("k", k);
        fboNeighborMax.getTextureReference().draw(0, 0);
        tileMaxShader.end();
        fboTileMax.end();
        
        // Neighbor Max
        fboNeighborMax.begin();
        neighborMaxShader.begin();
        fboTileMax.draw(0, 0);
        neighborMaxShader.end();
        fboNeighborMax.end();
    }
    
    void begin(ofCamera& cam)
    {
        cam.begin();
        cam.end();
        farClip = cam.getFarClip();
        modelviewProjectionMatrix = cam.getModelViewProjectionMatrix(ofRectangle(0, 0, w, h));
        renderPreparePass(cam);
        fboRead.begin();
        ofClear(0);
        cam.begin();
    }
    
    void end(ofCamera& cam, bool autoDraw = true)
    {
        cam.end();
        fboRead.end();
        
        fboWrite.begin();
        ofClear(0);
        reconstructionShader.begin();
        reconstructionShader.setUniform1f("farClip", farClip);
        reconstructionShader.setUniform1f("k", k);
        reconstructionShader.setUniform1i("S", settings.S);
        reconstructionShader.setUniform2f("viewport", w, h);
        reconstructionShader.setUniformTexture("texVelocity", fbo.getTextureReference(0), 1);
        reconstructionShader.setUniformTexture("linearDepth", fbo.getTextureReference(1), 2);
        reconstructionShader.setUniformTexture("neighborMax", fboNeighborMax.getTextureReference(), 3);
        
        fboRead.getTextureReference().draw(0, 0);
        reconstructionShader.end();
        fboWrite.end();
        
        if (autoDraw) {
            draw();
        }
    }
    
    void draw()
    {
        fboWrite.draw(0, 0);
    }
    
    void debugDraw()
    {
        float w2 = ofGetViewportWidth();
        float h2 = ofGetViewportHeight();
        float ws = w2*0.25;
        float hs = h2*0.25;
        
        fbo.getTextureReference(0).draw(0, hs*3, ws, hs);
        fbo.getTextureReference(1).draw(ws, hs*3, ws, hs);
        fboTileMax.draw(ws*2, hs*3, ws, hs);
        fboNeighborMax.draw(ws*3, hs*3, ws, hs);
    }
    
    void flush()
    {
        prevModelviewProjectionMatrix = modelviewProjectionMatrix;
        for (MotionBlurNode* node : nodes) {
            node->flush();
        }
        sphereNode.flush();
    }
};


class BoxMotionBlurNode : public MotionBlurNode {
    ofColor color;
    ofVec3f center;
    float radius;
    float velocity;
    ofVec3f rotationAxis;
    
public:
    BoxMotionBlurNode(ofColor c, ofVec3f cen, ofVec3f initialPos, float v)
    : color(c), center(cen)
    {
        setGlobalPosition(initialPos);
        radius = (initialPos - center).length();
        velocity = v;
        rotationAxis = (initialPos - center).crossed(ofVec3f(1,0,0)).normalized();
    }
    
    void update()
    {
        rotateAround(velocity, rotationAxis, center);
    }
    
protected:
    void customDraw()
    {
        ofPushStyle();
        ofSetColor(color);
        ofDrawBox(ofVec3f(0, 15, 0), 30);
        ofPopStyle();
    }
};
