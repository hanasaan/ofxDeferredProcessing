#pragma once
#include "ofMain.h"
#include "Processor.h"

// Part of this code is from James Acres's of-DeferredRendering
// https://github.com/jacres/of-DeferredRendering
namespace DeferredEffect {
    struct DeferredLight {
        DeferredLight() {}
        DeferredLight(const ofLight& light)
        {
            ambientColor = light.getAmbientColor();
            diffuseColor = light.getDiffuseColor();
            specularColor = light.getSpecularColor();
            position = light.getPosition();
        }
        
        ofFloatColor ambientColor;
        ofFloatColor diffuseColor;
        ofFloatColor specularColor;
        ofVec3f position;
        float intensity = 1.0;
        float radius = 200.0;
    };
    
    
    class DeferredLightingPass : public RenderPass {
    protected:
        vector<DeferredLight> lights;
        ofShader shader;
        float farClip;
        ofMatrix4x4 projectionMatrix;
        ofMatrix4x4 modelViewMatrix;
        bool isVFlipped;
    public:
        typedef shared_ptr<DeferredLightingPass> Ptr;
        
        DeferredLightingPass(const ofVec2f& sz);
        
        // Currently only point light is supported.
        void addLight(DeferredLight light) {
            lights.push_back(light);
        }
        DeferredLight& getLightRef(int index) {
            return lights[index];
        }
        void clear() { lights.clear(); }
        int getLightsSize() { return lights.size(); }
        vector<DeferredLight>& getLights() { return lights; }
        const vector<DeferredLight>& getLights() const { return lights; }
        
        void update(ofCamera& cam);
        void render(ofFbo& readFbo, ofFbo& writeFbo, GBuffer& gbuffer);
    };
}
