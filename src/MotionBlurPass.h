//
// Created by Yuya Hanai, https://github.com/hanasaan
//

#pragma once
#include "ofMain.h"
#include "Processor.h"

namespace DeferredEffect {
    class MotionBlurPass : public RenderPass {
    public:
        struct Settings {
            float exposureTime;
            int S;
            Settings() {
                exposureTime = 0.03f;
                S = 9;
            }
        } settings;
        
    private:
        float k;
        
        ofFbo fboTileMax;
        ofFbo fboNeighborMax;
        
        ofShader tileMaxShader;
        ofShader neighborMaxShader;
        ofShader reconstructionShader;
        
        float farClip;
    public:
        typedef shared_ptr<MotionBlurPass> Ptr;
        
        MotionBlurPass(const ofVec2f& sz, float k = 20);
        
        void update(ofCamera& cam);
        void render(ofFbo& readFbo, ofFbo& writeFbo, GBuffer& gbuffer);
    };
}
