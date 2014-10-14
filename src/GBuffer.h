//
// Created by Yuya Hanai, https://github.com/hanasaan
//
#pragma once
#include "ofMain.h"

// Part of this code is from James Acres's of-DeferredRendering
// https://github.com/jacres/of-DeferredRendering
namespace DeferredEffect {

    // Note : This class is thread unsafe.
    static ofShader* currentShader = NULL;

    class GBuffer;

    class GBufferObject : public ofNode
    {
    private:
        ofMatrix4x4 prevGlobalTransformMatrix;
    public:
        void flush();
        void drawToGBuffer(bool autoFlush = true);
    protected:
        virtual void customDraw() = 0;
    };


    class GBuffer
    {
    private:
        ofFbo fbo;
        ofShader shader;
        ofShader debugShader;
        ofMatrix4x4 prevModelviewProjectionMatrix;
    public:
        enum Mode {
            MODE_GEOMETRY,
            MODE_LIGHT
        };
        enum BufferType {
            TYPE_ALBEDO = 0,
            TYPE_NORMAL_DEPTH = 1,
            TYPE_VELOCITY = 2,
            TYPE_LIGHT_PASS = 3
        };
        
        GBuffer() {}
        
        void setup(int w = ofGetWidth(), int h = ofGetHeight());
        void begin(ofCamera& cam, Mode mode = MODE_GEOMETRY);
        void end();
        void debugDraw();
        ofTexture& getTexture(int index) {
            return fbo.getTextureReference(index);
        }
        ofFbo& getFbo() {return fbo;}
    };

}