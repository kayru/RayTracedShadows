#version 450

// Generate a fullscreen triangle
void main()
{
	if 		(gl_VertexIndex == 0) 	gl_Position = vec4(-3.0, -1.0, 1.0, 1.0);
	else if	(gl_VertexIndex == 1) 	gl_Position = vec4(1.0, -1.0, 1.0, 1.0);
	else 							gl_Position = vec4(1.0, 3.0, 1.0, 1.0);
}
