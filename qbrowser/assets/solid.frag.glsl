precision mediump float;

uniform vec4 inRect;
uniform vec4 fragColor;
uniform float cornerRadius;

varying vec2 fragPos;

void main() {

    vec2 p = fragPos - (inRect.xy + inRect.zw * 0.5);  // center the rectangle
    vec2 halfSize = inRect.zw * 0.5;

    // Distance to box with rounded corners
    vec2 d = abs(p) - (halfSize - vec2(cornerRadius));
    float dist = length(max(d, 0.0));

    float a = 1.0;
    if (dist > cornerRadius)
        a = clamp(1.0 + cornerRadius - dist, 0.0, 1.0);

    gl_FragColor.rgb = fragColor.rgb / 256.0;
    gl_FragColor.a = a * (fragColor.a / 256.0);
}
