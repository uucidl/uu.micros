// Coordinate system: Z up, right-handed.

#version 150

in vec3 vertex;
in vec3 normal;

out vec3 v_incidentLightVector;
out vec3 v_normal;
out vec3 v_vertexToEyeVector;

uniform vec3 iResolution;
uniform float iGlobalTime;
uniform vec3 iObjectCenterPosition;

const float TAU =
        6.2831853071795864769252867665590057683943387987502116419498891846156328125724179972560696506842341359f;

void main ()
{
        float r = 8.0f;
        vec3 ceilingLight = vec3(1.32, 1.70, 2.0);
        vec3 cameraCenter = vec3(
                                    r*sin(TAU*iGlobalTime/8.0),
                                    r*cos(TAU*iGlobalTime/8.0f),
                                    r
                            );
        vec3 cameraToObject = vec3(0.0, 0.0, 1.0) - cameraCenter;
        vec3 cameraToSky = vec3(0.0, 0.0, 1.0);

        vec3 cameraDirection = normalize(cameraToObject);
        vec3 cameraUp = normalize(cross(cameraToSky, cameraToObject));
        vec3 cameraRight = normalize(cross(cameraDirection, cameraUp));

        mat4 cameraTranslation = mat4(1.0f , 0.0f, 0.0f, 0.0f,
                                      0.0f, 1.0f, 0.0f, 0.0f,
                                      0.0f, 0.0f, 1.0f, 0.0f,
                                      -cameraCenter.x, -cameraCenter.y, -cameraCenter.z, 1.0f
                                     );
        mat4 cameraRotation = mat4(
                                      cameraUp.x, cameraRight.x, cameraDirection.x, 0.0f,
                                      cameraUp.y, cameraRight.y, cameraDirection.y, 0.0f,
                                      cameraUp.z, cameraRight.z, cameraDirection.z, 0.0f,
                                      0.0f, 0.0f, 0.0f, 1.0f
                              );
        mat4 worldToEye = cameraRotation * cameraTranslation;

        float focalLength = 1.0 / tan(TAU/6.0 / 2.0);
        float nearPlane = 1.0f;
        float farPlane = 32.0f;
        float aspectRatio = iResolution.y/iResolution.x;

        mat4 eyeToScreen = mat4(
                focalLength, 0.0f, 0.0f, 0.0f,
                0.0f, focalLength/aspectRatio, 0.0f, 0.0f,
                0.0f, 0.0f, (farPlane + nearPlane)/(farPlane - nearPlane), 1.0f,
                0.0f, 0.0f, -2*farPlane*nearPlane / (farPlane - nearPlane), 0.0f
        );

        vec3 objectVertex = vertex + iObjectCenterPosition;
        vec4 affineVertex = vec4(objectVertex.xyz, 1.0f);

        vec3 incidentLightVector = normalize(objectVertex - ceilingLight);
        vec3 reflectedLightVector = normalize(incidentLightVector - 2 * dot(
                        incidentLightVector,
                        normal)*normal);

        mat3 worldVectorToEyeVector = mat3(worldToEye);
        v_normal = worldVectorToEyeVector * normal;
        v_incidentLightVector = worldVectorToEyeVector * (objectVertex -
                                ceilingLight);
        v_vertexToEyeVector = worldVectorToEyeVector * (cameraCenter - objectVertex);

        gl_Position = eyeToScreen * worldToEye * affineVertex;
}
