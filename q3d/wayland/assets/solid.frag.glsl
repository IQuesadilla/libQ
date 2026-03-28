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

    if (dist > cornerRadius)
        discard;

    gl_FragColor = fragColor / 256.0;
}
