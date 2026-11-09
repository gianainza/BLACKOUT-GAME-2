#version 460

in vec4 vCol;
in vec2 TexCoord;
in vec3 Normal;
in vec3 FragPos;
in vec4 DirectionalLightSpacePos;

out vec4 colour;

const int MAX_POINT_LIGHTS = 3;      // must match CommonValues.h
const int MAX_SPOT_LIGHTS = 4;

struct Light
{
	vec3 colour;
	float ambientIntensity;
	float diffuseIntensity;
};

struct DirectionalLight
{
	Light base;
	vec3 direction;
};

struct PointLight
{
	Light base;
	vec3 position;
	float constant;
	float linear;
	float exponent;
};

struct SpotLight
{
	PointLight base;
	vec3 direction;
	float edge;
};

struct OmniShadowMap
{
	samplerCube shadowMap;
	float farPlane;
};

struct Material
{
	float specularIntensity;
	float shininess;
};

uniform int pointLightCount;
uniform int spotLightCount;

uniform DirectionalLight directionalLight;
uniform PointLight pointLights[MAX_POINT_LIGHTS];
uniform SpotLight spotLights[MAX_SPOT_LIGHTS];

uniform sampler2D theTexture;
uniform sampler2D directionalShadowMap;
uniform OmniShadowMap omniShadowMaps[MAX_POINT_LIGHTS + MAX_SPOT_LIGHTS];

uniform Material material;

uniform vec3 eyePosition;

// ---- OBSOLETE additions -------------------------------------------------------------------------
uniform float omniBias;          // world units. Was a hard-coded 0.05; a uniform so the demo can break it on purpose
uniform vec3  materialColour;    // tint multiplied into the texture
uniform float emissive;          // self-lit amount (terminal lamps, scanner beams)
uniform float alphaMul;          // < 1 for the additive beams and strike columns
uniform int   debugMode;         // 0 normal | 1 "what the game believes" overlay | 2 probe output (illum, mask)
uniform float litThreshold;
uniform int   decalLight;        // -1 = normal draw. >= 0: a scanner floor marker, shown only where that light can see (not in shadow)

// Gameplay accounting. The game asks "is the droid lit?"; the CPU answers in LitTest.cpp and this shader
// answers the same question per pixel, so the two can be compared (probe, key G / F9).
//   illum_i = (1 - shadow_i) * cone_i * diffuseIntensity_i / attenuation_i        (no normal term, no colour)
//   mask bit g is set when the cone covers the pixel and the shadow factor is below 0.5
// g numbers the gameplay lights: drones first, then scanners (spot 0, the droid's own lamp, is skipped).
float gIllum = 0.0;
float gMask  = 0.0;
float gVis[MAX_POINT_LIGHTS + MAX_SPOT_LIGHTS];      // (1 - shadow) per gameplay light, used by the scanner floor marker

vec3 sampleOffsetDirections[20] = vec3[]
(
   vec3( 1,  1,  1), vec3( 1, -1,  1), vec3(-1, -1,  1), vec3(-1,  1,  1),
   vec3( 1,  1, -1), vec3( 1, -1, -1), vec3(-1, -1, -1), vec3(-1,  1, -1),
   vec3( 1,  1,  0), vec3( 1, -1,  0), vec3(-1, -1,  0), vec3(-1,  1,  0),
   vec3( 1,  0,  1), vec3(-1,  0,  1), vec3( 1,  0, -1), vec3(-1,  0, -1),
   vec3( 0,  1,  1), vec3( 0, -1,  1), vec3( 0, -1, -1), vec3( 0,  1, -1)
);

float CalcDirectionalShadowFactor(DirectionalLight light)
{
	vec3 projCoords = DirectionalLightSpacePos.xyz / DirectionalLightSpacePos.w;
	projCoords = (projCoords * 0.5) + 0.5;

	float current = projCoords.z;

	vec3 normal = normalize(Normal);
	vec3 lightDir = normalize(directionalLight.direction);

	float bias = max(0.05 * (1.0 - dot(normal, -lightDir)), 0.0005);

	float shadow = 0.0;
	vec2 texelSize = 1.0 / textureSize(directionalShadowMap, 0);
	for(int x = -1; x <= 1; ++x)
	{
		for(int y = -1; y <= 1; ++y)
		{
			float pcfDepth = texture(directionalShadowMap, projCoords.xy + vec2(x,y) * texelSize).r;
			shadow += current - bias > pcfDepth ? 1.0 : 0.0;
		}
	}

	shadow /= 9.0;

	if(projCoords.z > 1.0)
	{
		shadow = 0.0;
	}

	return shadow;
}

float CalcOmniShadowFactor(PointLight light, int shadowIndex)
{
	vec3 fragToLight = FragPos - light.position;
	float currentDepth = length(fragToLight);

	float shadow = 0.0;
	float bias = omniBias;
	int samples = 20;

	float viewDistance = length(eyePosition - FragPos);
	float diskRadius = (1.0 + (viewDistance/omniShadowMaps[shadowIndex].farPlane)) / 25.0;

	for(int i = 0; i < samples; i++)
	{
		float closestDepth = texture(omniShadowMaps[shadowIndex].shadowMap, fragToLight + sampleOffsetDirections[i] * diskRadius).r;
		closestDepth *= omniShadowMaps[shadowIndex].farPlane;
		if(currentDepth -  bias > closestDepth)
		{
			shadow += 1.0;
		}
	}

	shadow /= float(samples);
	return shadow;
}

vec4 CalcLightByDirection(Light light, vec3 direction, float shadowFactor)
{
	vec4 ambientColour = vec4(light.colour, 1.0f) * light.ambientIntensity;

	// direction is the way the light TRAVELS, so the surface faces the light when N . (-direction) > 0.
	// (Activity J used +direction and relied on its demo meshes having inward-pointing normals.)
	float diffuseFactor = max(dot(normalize(Normal), -normalize(direction)), 0.0f);
	vec4 diffuseColour = vec4(light.colour * light.diffuseIntensity * diffuseFactor, 1.0f);

	vec4 specularColour = vec4(0, 0, 0, 0);

	if(diffuseFactor > 0.0f)
	{
		vec3 fragToEye = normalize(eyePosition - FragPos);
		vec3 reflectedVertex = normalize(reflect(direction, normalize(Normal)));

		float specularFactor = dot(fragToEye, reflectedVertex);
		if(specularFactor > 0.0f)
		{
			specularFactor = pow(specularFactor, material.shininess);
			specularColour = vec4(light.colour * material.specularIntensity * specularFactor, 1.0f);
		}
	}

	return (ambientColour + (1.0 - shadowFactor) * (diffuseColour + specularColour));
}

vec4 CalcDirectionalLight()
{
	float shadowFactor = CalcDirectionalShadowFactor(directionalLight);
	return CalcLightByDirection(directionalLight.base, directionalLight.direction, shadowFactor);
}

vec4 CalcPointLightCore(PointLight pLight, int shadowIndex, float cone, int gameIndex)
{
	vec3 direction = FragPos - pLight.position;
	float distance = length(direction);
	direction = normalize(direction);

	float shadowFactor = CalcOmniShadowFactor(pLight, shadowIndex);

	vec4 colour = CalcLightByDirection(pLight.base, direction, shadowFactor);
	float attenuation = pLight.exponent * distance * distance +
						pLight.linear * distance +
						pLight.constant;

	if (gameIndex >= 0 && cone > 0.0 && pLight.base.diffuseIntensity > 0.0)
	{
		gVis[gameIndex] = 1.0 - shadowFactor;
		gIllum += (1.0 - shadowFactor) * cone * pLight.base.diffuseIntensity / attenuation;
		if (shadowFactor < 0.5) gMask += float(1 << gameIndex);
	}

	return (colour * cone / attenuation);
}

vec4 CalcPointLight(PointLight pLight, int shadowIndex)
{
	return CalcPointLightCore(pLight, shadowIndex, 1.0, shadowIndex);
}

vec4 CalcSpotLight(SpotLight sLight, int shadowIndex)
{
	vec3 rayDirection = normalize(FragPos - sLight.base.position);
	float slFactor = dot(rayDirection, sLight.direction);

	if(slFactor > sLight.edge)
	{
		float cone = 1.0f - (1.0f - slFactor)*(1.0f/(1.0f - sLight.edge));
		// shadowIndex = pointLightCount + spotIndex; spot 0 is the droid's lamp and is not a gameplay light
		int gameIndex = (shadowIndex > pointLightCount) ? shadowIndex - 1 : -1;
		return CalcPointLightCore(sLight.base, shadowIndex, cone, gameIndex);
	} else {
		return vec4(0, 0, 0, 0);
	}
}

vec4 CalcPointLights()
{
	vec4 totalColour = vec4(0, 0, 0, 0);
	for(int i = 0; i < pointLightCount; i++)
	{
		totalColour += CalcPointLight(pointLights[i], i);
	}

	return totalColour;
}

vec4 CalcSpotLights()
{
	vec4 totalColour = vec4(0, 0, 0, 0);
	for(int i = 0; i < spotLightCount; i++)
	{
		totalColour += CalcSpotLight(spotLights[i], i + pointLightCount);
	}

	return totalColour;
}

void main()
{
	for (int k = 0; k < MAX_POINT_LIGHTS + MAX_SPOT_LIGHTS; ++k) gVis[k] = 0.0;

	vec4 finalColour = CalcDirectionalLight();
	finalColour += CalcPointLights();
	finalColour += CalcSpotLights();

	vec4 tex = texture(theTexture, TexCoord);
	vec3 base = tex.rgb * materialColour;
	colour = vec4(base * finalColour.rgb + base * emissive, tex.a * alphaMul);
	
	// R2 FIX: Soften the volumetric beams and fade them out at a distance
	if (alphaMul < 1.0 && decalLight == -1) {
		vec3 viewDir = normalize(eyePosition - FragPos);
		
		// 1. Fade the edges based on the view angle
		float edgeFade = smoothstep(0.0, 1.0, abs(dot(normalize(Normal), viewDir)));
		
		// 2. made the whole beam fade out if it gets too far from the camera
		float distToCamera = length(eyePosition - FragPos);
		float distanceFade = clamp(1.0 - (distToCamera / 18.0), 0.0, 1.0);
		
		
		colour.rgb *= edgeFade * distanceFade * 1.2;
	}

	if (decalLight >= 0 && debugMode == 0)
	{
		// scanner floor marker: pure glow, faded out where a blocker hides the floor from that scanner
		colour = vec4(base * emissive * gVis[decalLight], tex.a * alphaMul);
	}

	if (debugMode == 1)
	{
		// the game's opinion, painted on the world: red = scanned (illum >= threshold), blue = hidden
		colour = vec4(mix(base * finalColour.rgb, (gIllum >= litThreshold) ? vec3(1.0, 0.1, 0.05) : vec3(0.05, 0.1, 0.5), 0.55), 1.0);
	}
	else if (debugMode == 2)
	{
		colour = vec4(gIllum, gMask, 0.0, 1.0);          // read back by the CPU probe
	}
}
