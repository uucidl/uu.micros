#version 150

in vec4 v_ambientLight;
in vec3 v_incidentLightVector;
in vec3 v_normal;
in vec3 v_vertexToEyeVector;

out vec4 oColor;

void main ()
{
        vec3 incidentLightVector = normalize(v_incidentLightVector);
        vec3 normal = normalize(v_normal);
        vec3 reflectedLightVector = normalize(incidentLightVector - 2 * dot(
                        incidentLightVector,
                        normal)*normal);
        vec3 vertexToEyeVector = normalize(v_vertexToEyeVector);
        float environmentalDiffuse = 0.54321f;
        float specularCoeff = 0.42f;
        float shininess = 8.20f;

        float cos_thetas = max(0.0f, dot(vertexToEyeVector, reflectedLightVector));

        float L = 1.0f;

        vec4 Cdiffuse = vec4(0.90, 0.92, 0.98, 1.0);
        vec4 Cspecular = vec4(0.88, 0.93, 1.0f, 1.0);

        oColor = Cdiffuse*mix(L*max(0.0f,
                                    dot(reflectedLightVector,
                                        normal)),
                              1.0f,
                              environmentalDiffuse)
                 + Cspecular * specularCoeff * L * (cos_thetas / (shininess - shininess *
                                 cos_thetas
                                 +cos_thetas));
}
