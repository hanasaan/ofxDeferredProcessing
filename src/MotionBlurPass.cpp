//
// Created by Yuya Hanai, https://github.com/hanasaan
//

#include "MotionBlurPass.h"
#define STRINGIFY(A) #A
using namespace DeferredEffect;

static inline void createShaderWithHeader(ofShader& s, const string& str) {
    stringstream ss;
    ss << "#version 120" << endl;
    ss << "#extension GL_EXT_gpu_shader4 : enable" << endl;
    ss << str;
    s.setupShaderFromSource(GL_FRAGMENT_SHADER, ss.str());
    s.linkProgram();
}

MotionBlurPass::MotionBlurPass(const ofVec2f& sz, float k) : RenderPass(sz, "MotionBlurPass"), k(k) {
    farClip = 1000.0f;
    fboTileMax.allocate(sz.x / k, sz.y / k, GL_RG8);
    fboNeighborMax.allocate(sz.x / k, sz.y / k, GL_RG8);
    fboTileMax.getTextureReference().setTextureMinMagFilter(GL_NEAREST, GL_NEAREST);
    fboNeighborMax.getTextureReference().setTextureMinMagFilter(GL_NEAREST, GL_NEAREST);
    
    // This is based on the paper "A Reconstruction Filter for Plausible Motion Blur"
    // http://graphics.cs.williams.edu/papers/MotionBlurI3D12/
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
     uniform sampler2DRect normalDepth;
     uniform sampler2DRect neighborMax;
     uniform float k;
     uniform float farClip;
     uniform int S;
     uniform vec2 viewport;
     uniform float exposureTime;
     uniform float fps;
     
     const float SOFT_Z_EXTENT = 0.1;
     
     // convert to pixel space
     vec2 decodeVelocity(const in vec2 v) {
         vec2 vd = v;
         vd = (vd - vec2(127.0/255.0));
         vd = (vd * vd) * sign(vd) * viewport;
         
         // clamp
         float length = clamp(exposureTime * fps * length(vd), 0.0, k);
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
             float zx = farClip * texelFetch2DRect(normalDepth, ivec2(uv)).a;
             float zy = farClip * texelFetch2DRect(normalDepth, ivec2(Y)).a;
             vec2 vy = decodeVelocity(texelFetch2DRect(texVelocity, ivec2(Y)).xy);
             
             float f = softDepthCompare(zx, zy);
             float b = softDepthCompare(zy, zx);
             float ay = b * cone(X, Y, vy) + f * cone(X, Y, v) + cylinder(Y, X, vy) * cylinder(X, Y, v) * 2.0;
             weight += ay;
             sum += texture2DRect(tex, Y) * ay;
         }
         gl_FragColor = sum / weight;
     }
     );
    
    createShaderWithHeader(tileMaxShader, tileMaxFragShader);
    createShaderWithHeader(neighborMaxShader, neighborMaxFragShader);
    createShaderWithHeader(reconstructionShader, reconstructionFragShader);
}

void MotionBlurPass::update(ofCamera& cam) {
    farClip = cam.getFarClip();
}

void MotionBlurPass::render(ofFbo& readFbo, ofFbo& writeFbo, GBuffer& gbuffer) {
    // Tile Max
    fboTileMax.begin();
    tileMaxShader.begin();
    tileMaxShader.setUniformTexture("texVelocity", gbuffer.getTexture(GBuffer::TYPE_VELOCITY), 1);
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
    
    writeFbo.begin();
    ofClear(0);
    reconstructionShader.begin();
    reconstructionShader.setUniform1f("farClip", farClip);
    reconstructionShader.setUniform1f("k", k);
    reconstructionShader.setUniform1i("S", settings.S);
    reconstructionShader.setUniform1f("exposureTime", settings.exposureTime);
    reconstructionShader.setUniform1f("fps", ofGetFrameRate());
    reconstructionShader.setUniform2f("viewport", size.x, size.y);
    reconstructionShader.setUniformTexture("texVelocity", gbuffer.getTexture(GBuffer::TYPE_VELOCITY), 1);
    reconstructionShader.setUniformTexture("normalDepth", gbuffer.getTexture(GBuffer::TYPE_NORMAL_DEPTH), 2);
    reconstructionShader.setUniformTexture("neighborMax", fboNeighborMax.getTextureReference(), 3);
    
    readFbo.draw(0, 0);
    reconstructionShader.end();
    writeFbo.end();
}