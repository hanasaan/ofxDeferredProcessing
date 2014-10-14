#pragma once
#include "ofMain.h"
#include "GBuffer.h"

// This code is modified from Neil Mendoza's ofxPostProcessing (BSD lisence)
// https://github.com/neilmendoza/ofxPostProcessing
namespace DeferredEffect {
    using namespace tr1;
    
    class RenderPass {
    public:
        typedef shared_ptr<RenderPass> Ptr;
        
        RenderPass(const ofVec2f& sz, const string& n) : size(sz), name(n), enabled(true) {}
        
        virtual void update(ofCamera& cam) = 0;
        virtual void render(ofFbo& readFbo, ofFbo& writeFbo, GBuffer& gbuffer) = 0;
        
        void setEnabled(bool enabled) { this->enabled = enabled; }
        bool getEnabled() const { return enabled; }
        
        // for GUI
        bool& getEnabledRef() { return enabled; }
        string getName() const { return name; }
        
    protected:
        void texturedQuad(float x, float y, float width, float height, float s = 1.0, float t = 1.0);
        
        string name;
        bool enabled;
        ofVec2f size;
    };
    
    class Processor : public ofBaseDraws {
    public:
        typedef shared_ptr<Processor> Ptr;
        
        void init(unsigned width = ofGetWidth(), unsigned height = ofGetHeight());
        
        void beginGbuffer(ofCamera& cam) {
            gbuffer.begin(cam);
        }
        void endGbuffer() {
            gbuffer.end();
        }
        
        void begin(ofCamera& cam);
        void end(bool autoDraw = true);
        
        // float rather than int and not const to override ofBaseDraws
        void draw(float x = 0.f, float y = 0.f);
        void draw(float x, float y, float w, float h);
        float getWidth() { return width; }
        float getHeight() { return height; }
        
        void debugDraw();
        
        template<class T>
        shared_ptr<T> createPass()
        {
            shared_ptr<T> pass = shared_ptr<T>(new T(ofVec2f(width, height)));
            passes.push_back(pass);
            return pass;
        }
        
        ofTexture& getProcessedTextureReference();
        
        // advanced
        void process(ofFbo& raw);
        
        unsigned size() const { return passes.size(); }
        RenderPass::Ptr operator[](unsigned i) const { return passes[i]; }
        vector<RenderPass::Ptr>& getPasses() { return passes; }
        unsigned getNumProcessedPasses() const { return numProcessedPasses; }
        
        ofFbo& getRawRef() { return raw; }
        
        GBuffer& getGBufferRef() { return gbuffer; }
    private:
        void process();
        
        unsigned currentReadFbo;
        unsigned numProcessedPasses;
        unsigned width, height;
        
        GBuffer gbuffer;
        ofFbo raw;
        ofFbo pingPong[2];
        vector<RenderPass::Ptr> passes;
    };
   
}