#include "DeferredLightingPass.h"

#define STRINGIFY(A) #A
using namespace DeferredEffect;

DeferredLightingPass::DeferredLightingPass(const ofVec2f& sz) : RenderPass(sz, "DeferredLightingPass")
{
    // ToDo
    
    // Shader code is modified from James Acres's of-DeferredRendering
    // https://github.com/jacres/of-DeferredRendering
    string pontLightVertShader = STRINGIFY
    (
     varying vec2 v_texCoord;
     void main(void)
    {
        v_texCoord = gl_MultiTexCoord0.st;
        gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;
    }
    );
    
    string pontLightFragShader = STRINGIFY
    (
     // deferred g buffers
     uniform sampler2DRect u_albedoTex;  // albedo (diffuse without lighting)
     uniform sampler2DRect u_normalAndDepthTex;  // view space normal and linear depth
     
     // LIGHTS
     uniform int u_numLights;
     uniform vec3 u_lightPosition;
     uniform vec4 u_lightAmbient;
     uniform vec4 u_lightDiffuse;
     uniform vec4 u_lightSpecular;
     uniform vec3 u_lightAttenuation;
     uniform float u_lightIntensity;
     uniform float u_lightRadius;
     
     uniform float u_farDistance;
     uniform mat4 u_inverseProjection;
     uniform vec4 u_viewport;
     
     varying vec2 v_texCoord;
     
     struct material {
         vec4 ambient;
         vec4 diffuse;
         vec4 specular;
         float shininess;
     };
     
     const material material1 = material(
                                         vec4(0.1, 0.1, 0.1, 1.0),
                                         vec4(1.0, 1.0, 1.0, 1.0),
                                         vec4(1.0, 1.0, 1.0, 1.0),
                                         127.0
                                         );
     
     const vec4 ambientGlobal = vec4(0.05, 0.05, 0.05, 1.0);
     
     void main(void)
    {
        vec2 texCoord = v_texCoord;
        
        vec3 albedo = texture2DRect(u_albedoTex, texCoord.st).rgb;
        
        //convert from screen to camera
        vec4 screenpos = vec4(1.0);
        screenpos.x = 2.0 * (texCoord.x - u_viewport.x) / u_viewport.z - 1.0;
        screenpos.y = 1.0 - 2.0 *(texCoord.y - u_viewport.y) / u_viewport.w;
        
        //get inverse
        vec4 v_vertex = u_inverseProjection * screenpos;
        
        float linearDepth = texture2DRect(u_normalAndDepthTex, texCoord.st).a;
        
        // vector to far plane
        vec3 viewRay = vec3(v_vertex.xy * (-u_farDistance/v_vertex.z), -u_farDistance);
        //viewRay.y = -viewRay.y;
        // scale viewRay by linear depth to get view space position
        vec3 vertex = viewRay * linearDepth;
        
        vec3 normal = texture2DRect(u_normalAndDepthTex, texCoord.st).xyz;
        
        vec4 ambient = vec4(0.0, 0.0, 0.0, 1.0);
        vec4 diffuse = vec4(0.0, 0.0, 0.0, 1.0);
        vec4 specular = vec4(0.0, 0.0, 0.0, 1.0);
        
        vec3 lightDir = u_lightPosition - vertex;
        vec3 R = normalize(reflect(lightDir, normal));
        vec3 V = normalize(vertex);
        
        float lambert = max(dot(normal, normalize(lightDir)), 0.0);
        
        if (lambert > 0.0) {
            float distance = length(lightDir);
            
            if (distance <= u_lightRadius) {
                // different attenuation methods - we have to stay within bounding radius, so it's a bit trickier than forward rendering
                //      float attenuation = 1.0 - distance/u_lightRadius;
                //      float attenuation = 1.0 / (u_lightAttenuation.x + u_lightAttenuation.y * distance + u_lightAttenuation.z * distance * distance);
                //      //attenuation = max(1.0, attenuation);
                
                //      (1-(x/r)^2)^3
                //      float attenuation = (1.0 - pow(pow(distance/u_lightRadius, 2), 3));
                
                float distancePercent = distance/u_lightRadius;
                float damping_factor = 1.0 - pow(distancePercent, 3.0);
                float attenuation = 1.0/(u_lightAttenuation.x +
                                         u_lightAttenuation.y * distance +
                                         u_lightAttenuation.z * distance * distance);
                attenuation *= damping_factor;
                
                vec4 diffuseContribution = material1.diffuse * u_lightDiffuse * lambert;
                diffuseContribution *= u_lightIntensity;
                diffuseContribution *= attenuation;
                
                vec4 specularContribution = material1.specular * u_lightSpecular * pow(max(dot(R, V), 0.0), material1.shininess);
                specularContribution *= u_lightIntensity;
                specularContribution *= attenuation;
                
                diffuse += diffuseContribution;
                specular += specularContribution;
            }
        }
        
        vec4 final_color = vec4(ambient + diffuse + specular);
        gl_FragColor = vec4(final_color.rgb * albedo, 1.0);
    }
     );

    shader.setupShaderFromSource(GL_VERTEX_SHADER, pontLightVertShader);
    shader.setupShaderFromSource(GL_FRAGMENT_SHADER, pontLightFragShader);
    shader.linkProgram();
}


void DeferredLightingPass::update(ofCamera& cam)
{
    farClip = cam.getFarClip();
    isVFlipped = cam.isVFlipped();
    ofRectangle viewport(0, 0, size.x, size.y);
    projectionMatrix = cam.getProjectionMatrix(viewport);
    modelViewMatrix = cam.getModelViewMatrix();
}

void DeferredLightingPass::render(ofFbo& readFbo, ofFbo& writeFbo, GBuffer& gbuffer)
{
    shader.begin();
    // pass in lighting info
    int numLights = lights.size();
    shader.setUniform1i("u_numLights", numLights);
    shader.setUniform1f("u_farDistance", farClip);
    shader.setUniform4f("u_viewport", 0, 0, size.x, size.y);
    shader.setUniformMatrix4f("u_inverseProjection", projectionMatrix.getInverse());
    shader.setUniform3f("u_lightAttenuation", 1, 0, 0);
    shader.setUniformTexture("u_albedoTex", gbuffer.getTexture(GBuffer::TYPE_ALBEDO), 1);
    shader.setUniformTexture("u_normalAndDepthTex", gbuffer.getTexture(GBuffer::TYPE_NORMAL_DEPTH), 2);
    shader.end();
    
    writeFbo.begin();
    ofClear(0);
    ofPushStyle();
    ofEnableBlendMode(OF_BLENDMODE_ADD);
    
    shader.begin();
    for (DeferredLight& light : lights) {
        ofVec3f lightPosInViewSpace = light.position * modelViewMatrix;
        shader.setUniform3fv("u_lightPosition", &lightPosInViewSpace.getPtr()[0]);
        shader.setUniform4fv("u_lightAmbient", light.ambientColor.v);
        shader.setUniform4fv("u_lightDiffuse", light.diffuseColor.v);
        shader.setUniform4fv("u_lightSpecular", light.specularColor.v);
        shader.setUniform1f("u_lightIntensity", 1.0f);
        shader.setUniform1f("u_lightRadius", 200.0f);
        texturedQuad(0, 0, size.x, size.y, size.x, size.y);
    }
    shader.end();
    
    ofPopStyle();
    writeFbo.end();
}