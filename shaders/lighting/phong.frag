#version 460

struct PointLight {
    vec4 position;
    vec4 color;
    mat4 mvps[6];
};

struct AreaLight {
    vec4 position;
    vec4 color;
    mat4 mvp;
};

// SSBOs
layout(binding = 0) buffer pointLights { PointLight pointLightsData[]; };
layout(binding = 1) buffer areaLights { AreaLight areaLightsData[]; };

// Texture data
layout(location = 3) uniform sampler2D colorMap;
layout(location = 4) uniform bool hasTexCoords;

// Camera position
layout(location = 5) uniform vec3 cameraPos;

// Lighting and shading parameter(s)
layout(location = 6) uniform float objectShininess = 10.0;
layout(location = 7) uniform float shadowFarPlane;

// Shadow map array(s)
layout(location = 8) uniform samplerCubeArrayShadow pointShadowTexArr;
layout(location = 9) uniform sampler2DArrayShadow areaShadowTexArr;

// Input from vertex shader
in vec3 fragPosition;
in vec3 fragNormal;
in vec2 fragTexCoord;

// Output
layout(location = 0) out vec4 fragColor;

/*****************************************************************************************************/

vec3 lambertianDiffuse(vec3 lightColor, vec3 lightPos, vec3 materialProps) {
    vec3 lightDir = normalize(lightPos - fragPosition);
    return dot(lightDir, fragNormal) * lightColor * materialProps;
}

vec3 phongSpecular(vec3 lightColor, vec3 lightPos, vec3 materialProps) {
    vec3 lightToSurface     = normalize(fragPosition - lightPos);
    vec3 surfaceToCamera    = normalize(cameraPos - fragPosition);
    vec3 reflection         = reflect(lightToSurface, fragNormal);

    float lightNormalDot    = dot(-lightToSurface, fragNormal);
    float reflectionViewDot = dot(reflection, surfaceToCamera);
    return  lightNormalDot > 0.0 && reflectionViewDot > 0.0 ?
            materialProps * pow(reflectionViewDot, objectShininess) * lightColor :
            vec3(0.0, 0.0, 0.0);
}

/*****************************************************************************************************/

// @param sampleCoord: Coordinates of fragment sample
// @param lightIdx: Index of the point light in the SSBO/shadow map
float samplePointShadow(vec3 sampleCoord, uint lightIdx) {
    vec3 lightToSample      = sampleCoord - pointLightsData[lightIdx].position.xyz;
    float clippedDistance   = length(lightToSample) / shadowFarPlane;
    vec4 texIdx             = vec4(lightToSample, lightIdx);

    const float EPSILON = 1e-3 * exp(4 * clippedDistance); // Adaptive epsilon for shadow acne
    return texture(pointShadowTexArr, texIdx, clippedDistance - EPSILON);
}

// @param sampleLightCoord: Homogeneous coordinates of fragment sample tranformed by MVP of shadow-casting light
// @param lightIdx: Index of the area light in the SSBO/shadow map
float sampleAreaShadow(vec4 sampleLightCoord, uint lightIdx) {
    // Divide by w because sampleLightCoord are homogeneous coordinates
    sampleLightCoord.xyz /= sampleLightCoord.w;

    // The resulting value is in NDC space (-1 to +1), we transform them to texture space (0 to 1).
    sampleLightCoord.xyz = sampleLightCoord.xyz * 0.5 + 0.5;

    // Add slight epsilon to fragment depth value used for comparison
    const float EPSILON = 1e-3;
    sampleLightCoord.z -= EPSILON;

    // Shadow map value from the corresponding shadow map position ()
    vec4 texcoord;
    texcoord.xyw    = sampleLightCoord.xyz;
    texcoord.z      = lightIdx;
    return texture(areaShadowTexArr, texcoord);
}

/*****************************************************************************************************/

void main() {
    const vec3 materialProps    = hasTexCoords ? texture(colorMap, fragTexCoord).rgb : vec3(1.0, 1.0, 1.0);
    fragColor = vec4(0.0, 0.0, 0.0, 0.0);

    // Accumulate lighting from point lights
    for (uint lightIdx = 0U; lightIdx < pointLightsData.length(); lightIdx++) {
        PointLight light    = pointLightsData[lightIdx];
        vec3 lightColor     = light.color.rgb;
        vec3 lightPosition  = light.position.xyz;
        
        float successFraction   = samplePointShadow(fragPosition, lightIdx);
        if (successFraction != 0.0) {
            vec3 diffuse    = lambertianDiffuse(lightColor, lightPosition, materialProps);
            vec3 specular   = phongSpecular(lightColor, lightPosition, materialProps);
            fragColor.rgb   += successFraction * (diffuse + specular);
        }
    }

    // Accumulate lighting from area lights
    for (uint lightIdx = 0U; lightIdx < areaLightsData.length(); lightIdx++) {
        AreaLight light     = areaLightsData[lightIdx];
        vec3 lightColor     = light.color.rgb;
        vec3 lightPosition  = light.position.xyz;

        vec4 fragLightCoord     = light.mvp * vec4(fragPosition, 1.0);
        float successFraction   = sampleAreaShadow(fragLightCoord, lightIdx);
        if (successFraction != 0.0) {
            vec3 diffuse    = lambertianDiffuse(lightColor, lightPosition, materialProps);
            vec3 specular   = phongSpecular(lightColor, lightPosition, materialProps);
            fragColor.rgb   += successFraction * (diffuse + specular);
        }
    }
}
