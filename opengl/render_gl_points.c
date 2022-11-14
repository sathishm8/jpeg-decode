#include<GL/glut.h>

void display() {
   unsigned char data[512][512] = { 0x99 };
   glClear(GL_COLOR_BUFFER_BIT);
   glColor3f(1.0, 0.0, 0.0);

   glBegin(GL_POINTS);
   for (int j = 0; j < 512; j++) {
      for (int i = 0; i < 512; i++) {
         int r = 200, g = 12, b = 50;
         r = g = b = data[0][0];
         glVertex2f(i, j);
         glColor3f((float)r / 255, (float)g / 255, (float)b / 255);
      }
   }
   glEnd();
   glFlush();
}

void init() {
   glClearColor(1.0, 1.0, 1.0, 1.0);
   glColor3f(1.0, 0.0, 0.0);
   glPointSize(1.0);
   glMatrixMode(GL_PROJECTION);
   glLoadIdentity();
   gluOrtho2D(0.0, 512.0, 0.0, 512.0);
}

void main(int argc, char** argv) {
   glutInit(&argc, argv);
   glutInitDisplayMode(GLUT_SINGLE | GLUT_RGB);
   glutInitWindowSize(512, 512);
   glutInitWindowPosition(0, 0);
   glutCreateWindow("512x512 points");
   glutDisplayFunc(display);
   init();
   glutMainLoop();
}
