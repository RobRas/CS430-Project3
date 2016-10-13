#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define PLANE 0
#define SPHERE 1
#define CAMERA 2
#define LIGHT 3

#define MAX_COLOR_VALUE 255

int line = 1;

typedef struct {
  unsigned char r, g, b;
} Pixel;

typedef struct {
  double width;
  double height;
} Camera;

typedef struct {
  int kind; // 0 = Plane, 1 = Sphere, 2 = Camera, 3 = Light
  double color[3];
  double position[3];
  union {
    struct {
      double normal[3];
    } plane;
    struct {
      double radius;
    } sphere;
    struct {
      double direction[3];
      double radialAtten[3];
      double angularAtten;
    } light;
  };
} Object;

Pixel* pixmap;
Camera** camera;
Object** objects;
Object** lights;

static inline double sqr(double v) {
  return v*v;
}

static inline void normalize(double* v) {
  double len = sqrt(sqr(v[0]) + sqr(v[1]) + sqr(v[2]));
  v[0] /= len;
  v[1] /= len;
  v[2] /= len;
}

static inline double dot(double* a, double* b) {
  return a[0]*b[0] + a[1]*b[1] + a[2]*b[2];
}

static inline double magnitude(double* v) {
  return sqrt(sqr(v[0]) + sqr(v[1]) + sqr(v[2]));
}

static inline double clamp(double value, double min, double max) {
  if (value < min) return min;
  if (value > max) return max;
  return value;
}


// Wraps the getc() function and provides error checking and
// number maintenance
int fnextc(FILE* json) {
  int c = fgetc(json);
#ifdef DEBUG
  printf("fnextc: '%c'\n", c);
#endif
  if (c == '\n') {
    line++;
  }
  if (c == EOF) {
    fprintf(stderr, "Error: Unexpected end of file on line number %d.\n", line);
    exit(1);
  }
  return c;
}

// fexpectc() checks that the next character in d. If it is not it
// emits and error.
void fexpectc(FILE* json, int d) {
  int c = fnextc(json);
  if (c == d) return;
  fprintf(stderr, "Error: Expected '%c' on line %d.\n", d, line);
  exit(1);
}

// skipWhitespace skips white space in the file.
void skipWhitespace(FILE* json) {
  int c = fnextc(json);
  while (isspace(c)) {
    c = fnextc(json);
  }
  ungetc(c, json);
}

// parseString gets the next string from the file handle and emits
// an error if a string can not be obtained.
char* nextString(FILE* json) {
  char buffer[129];
  int c = fnextc(json);
  if (c != '"') {
    fprintf(stderr, "Error: Expected string on line %d.\n", line);
    exit(1);
  }
  c = fnextc(json);
  int i = 0;
  while (c != '"') {
    if (i >= 128) {
      fprintf(stderr, "Error: Strings longer than 128 characters in length are not supported. See line %d.\n", line);
      exit(1);
    } else if (c == '\\') {
      fprintf(stderr, "Error: Strings with escape codes are not supperted. See line %d.\n", line);
      exit(1);
    } else if (c < 32 || c > 126) {
      fprintf(stderr, "Error: Strings may contain only ascii characters. See line %d.\n", line);
      exit(1);
    }
    buffer[i] = c;
    i++;
    c = fnextc(json);
  }
  buffer[i] = '\0';
  return strdup(buffer);
}

double nextNumber(FILE* json) {
  double value;
  fscanf(json, "%lf", &value);
  // Error check this...
  return value;
}

double* nextVector(FILE* json) {
  double* v = malloc(3 * sizeof(double));
  fexpectc(json, '[');
  skipWhitespace(json);
  v[0] = nextNumber(json);
  skipWhitespace(json);
  fexpectc(json, ',');
  skipWhitespace(json);
  v[1] = nextNumber(json);
  skipWhitespace(json);
  fexpectc(json, ',');
  skipWhitespace(json);
  v[2] = nextNumber(json);
  skipWhitespace(json);
  fexpectc(json, ']');
  return v;
}

void parseObject(FILE* json, int currentObject, int objectType) {
  int c;
  while (1) {
    c = fnextc(json);
    if (c == '}') {
      // Stop parsing this object
      break;
    } else if (c == ',') {
      skipWhitespace(json);
      char* key = nextString(json);
      skipWhitespace(json);
      fexpectc(json, ':');
      skipWhitespace(json);

      if (strcmp(key, "width") == 0) {
        if (objectType == CAMERA) {
          double w = nextNumber(json);
          if (w > 0) {
              camera[0]->width = w;
          } else {
            fprintf(stderr, "Camera width must be greater than 0.\n");
            exit(1);
          }
        } else {
          fprintf(stderr, "Error: Improper object field on line %d", line);
          exit(1);
        }
      } else if (strcmp(key, "height") == 0) {
        if (objectType == CAMERA) {
          double h = nextNumber(json);
          if (h > 0) {
              camera[0]->height = h;
          } else {
            fprintf(stderr, "Camera height must be greater than 0.\n");
            exit(1);
          }
        } else {
          fprintf(stderr, "Error: Improper object field on line %d", line);
          exit(1);
        }
      } else if (strcmp(key, "radius") == 0) {
        if (objectType == SPHERE) {
          double radius = nextNumber(json);
          if (radius >= 0) {
            objects[currentObject]->sphere.radius = radius;
          } else {
            fprintf(stderr, "Error: Radius cannot be less than 0.\n");
            exit(1);
          }
        }  else {
          fprintf(stderr, "Error: Improper object field on line %d", line);
          exit(1);
        }
      } else if (strcmp(key, "color") == 0) {
        if (objectType == PLANE || objectType == SPHERE) {
          double* v = nextVector(json);
          for (int i = 0; i < 3; i++) {
            objects[currentObject]->color[i] = v[i];
          }
        } else if (objectType == LIGHT) {
          double* v = nextVector(json);
          for (int i = 0; i < 3; i++) {
            lights[currentObject]->color[i] = v[i];
          }
        } else {
          fprintf(stderr, "Error: Improper object field on line %d", line);
          exit(1);
        }
      } else if (strcmp(key, "position") == 0) {
        if (objectType == PLANE || objectType == SPHERE) {
          double* v = nextVector(json);
          for (int i = 0; i < 3; i++) {
            objects[currentObject]->position[i] = v[i];
          }
        } else if (objectType == LIGHT) {
          double* v = nextVector(json);
          for (int i = 0; i < 3; i++) {
            lights[currentObject]->position[i] = v[i];
          }
        } else {
          fprintf(stderr, "Error: Improper object field on line %d", line);
          exit(1);
        }
      } else if (strcmp(key, "normal") == 0) {
        if (objectType == PLANE) {
          double* v = nextVector(json);
          normalize(v);
          for (int i = 0; i < 3; i++) {
            objects[currentObject]->plane.normal[i] = v[i];
          }
        } else {
          fprintf(stderr, "Error: Improper object field on line %d", line);
          exit(1);
        }
      } else if (strcmp(key, "direction") == 0) {
        if (objectType == LIGHT) {
          double* v = nextVector(json);
          normalize(v);
          for (int i = 0; i < 3; i++) {
            lights[currentObject]->light.direction[i] = v[i];
          }
        } else {
          fprintf(stderr, "Error: Improper object field on line %d", line);
          exit(1);
        }
      } else if (strcmp(key, "radial-a2") == 0) {
        if (objectType == LIGHT) {
          double rAtten2 = nextNumber(json);
          lights[currentObject]->light.radialAtten[2] = rAtten2;
        } else {
          fprintf(stderr, "Error: Improper object field on line %d", line);
          exit(1);
        }
      } else if (strcmp(key, "radial-a1") == 0) {
        if (objectType == LIGHT) {
          double rAtten1 = nextNumber(json);
          lights[currentObject]->light.radialAtten[1] = rAtten1;
        } else {
          fprintf(stderr, "Error: Improper object field on line %d", line);
          exit(1);
        }
      } else if (strcmp(key, "radial-a0") == 0) {
        if (objectType == LIGHT) {
          double rAtten0 = nextNumber(json);
          lights[currentObject]->light.radialAtten[0] = rAtten0;
        } else {
          fprintf(stderr, "Error: Improper object field on line %d", line);
          exit(1);
        }
      } else if (strcmp(key, "angular-a0") == 0) {
        if (objectType == LIGHT) {
          double aAtten = nextNumber(json);
          lights[currentObject]->light.angularAtten = aAtten;
        } else {
          fprintf(stderr, "Error: Improper object field on line %d", line);
          exit(1);
        }
      } else {
        fprintf(stderr, "Error: Unknown property, \"%s\", on line %d.\n", key, line);
        exit(1);
      }
      skipWhitespace(json);
    } else {
      fprintf(stderr, "Error: Unexpected value on line %d.\n", line);
      exit(1);
    }
  }
}

void parseJSON(char* fileName) {
  int c;
  FILE* json = fopen(fileName, "r");
  camera[0] = NULL;

  if (json == NULL) {
    fprintf(stderr, "Error: Could not open file \"%s\"\n", fileName);
    exit(1);
  }

  skipWhitespace(json);

  // Find the beginning of the list
  fexpectc(json, '[');

  skipWhitespace(json);

  int currentObject = 0;
  int currentLight = 0;
  while (1) {
    c = fnextc(json);
    if (c == ']') {
      fprintf(stderr, "Error: This is the worst scene file EVER.\n");
      fclose(json);
      return;
    } else if (c == '{') {
      skipWhitespace(json);

      // Parse the object
      char* key = nextString(json);
      if (strcmp(key, "type") != 0) {
        fprintf(stderr, "Error: Expected \"type\" key on line number %d.\n", line);
        exit(1);
      }

      skipWhitespace(json);

      fexpectc(json, ':');

      skipWhitespace(json);

      char* value = nextString(json);

      skipWhitespace(json);
      if (strcmp(value, "camera") == 0) {
        if (camera[0] == NULL) {
          camera[0] = malloc(sizeof(Camera));
          parseObject(json, currentObject, CAMERA);
        } else {
          fprintf(stderr, "Error: There should only be one camera per scene.\n");
          exit(1);
        }
      } else if (strcmp(value, "sphere") == 0) {
        objects[currentObject] = malloc(sizeof(Object));
        objects[currentObject]->kind = SPHERE;
        parseObject(json, currentObject, SPHERE);
        currentObject++;
      } else if (strcmp(value, "plane") == 0) {
        objects[currentObject] = malloc(sizeof(Object));
        objects[currentObject]->kind = PLANE;
        parseObject(json, currentObject, PLANE);
        currentObject++;
      } else if (strcmp(value, "light") == 0) {
        lights[currentLight] = malloc(sizeof(Object));
        lights[currentLight]->kind = LIGHT;
        parseObject(json, currentLight, LIGHT);
        currentLight++;
      } else {
        fprintf(stderr, "Error: Unknown type, \"%s\", on line number %d.\n", value, line);
        exit(1);
      }

      skipWhitespace(json);
      c = fnextc(json);
      if (c == ',') {
        skipWhitespace(json);
      } else if (c == ']') {
        if (camera[0] == NULL) {
          fprintf(stderr, "Error: Scene must contain a camera.\n");
          exit(1);
        }
        objects[currentObject] = NULL;
        lights[currentLight] = NULL;
        fclose(json);
        return;
      } else {
        fprintf(stderr, "Error: Expecting ',' or ']' on line %d.\n", line);
        exit(1);
      }
    } else {
      fprintf(stderr, "Error: Expecting '{' on line %d.\n", line);
      exit(1);
    }
  }
}

double planeIntersection(double* Ro, double* Rd, double* P, double* N) {
  double d = -dot(N, P);
  double Vd = dot(N, Rd);
  if (Vd == 0) return -1;
  double Vo = -(dot(N, Ro) + d);
  double t = Vo / Vd;
  if (t < 0) return -2;
  return t;
}

double sphereIntersection(double* Ro, double* Rd, double* P, double r) {
  double A = sqr(Rd[0]) + sqr(Rd[1]) + sqr(Rd[2]);
  double B = 2 * (Rd[0] * (Ro[0] - P[0]) + Rd[1] * (Ro[1] - P[1]) + Rd[2] * (Ro[2] - P[2]));
  double C = sqr(Ro[0] - P[0]) + sqr(Ro[1] - P[1]) + sqr(Ro[2] - P[2]) - sqr(r);

  double det = sqr(B) - 4 * A * C;
  if (det < 0) return -1;
  det = sqrt(det);
  double t0 = (-B - det) / (2 * A);
  if (t0 > 0) return t0;

  double t1 = (-B + det) / (2 * A);
  if (t1 > 0) return t1;

  return -1;
}

double angularAttenuation() {
  return 0;
}

double radiasAttenuation(double a2, double a1, double a0, double d) {
  return 1 / (a2 * square(d) + a1 * d + a0);
}

void createScene(int width, int height) {
  double cx = 0;
  double cy = 0;
  double h = camera[0]->height;
  double w = camera[0]->width;

  int M = height;
  int N = width;

  double pixheight = h / M;
  double pixwidth = w / N;

  for (int y = 0; y < M; y++) {
    for (int x = 0; x < N; x++) {
      double Ro[3] = {0, 0, 0};
      double Rd[3] = {
        cx - (w/2) + pixwidth * (x + 0.5),
        cy - (h/2) + pixheight * (y + 0.5),
        1
      };
      normalize(Rd);

      double closestT = INFINITY;
      Object* closestObject = NULL;
      for (int i = 0; objects[i] != NULL; i++) {
        double t = 0;

        switch(objects[i]->kind) {
          case PLANE:
            t = planeIntersection(Ro, Rd,
              objects[i]->position,
              objects[i]->plane.normal);
            break;
          case SPHERE:
            t = sphereIntersection(Ro, Rd,
              objects[i]->position,
              objects[i]->sphere.radius);
            break;
          default:
            fprintf(stderr, "Error: Object does not have an appropriate kind.");
            exit(1);
        }

        if (t > 0 && t < closestT) {
          closestT = t;
          closestObject = objects[i];
        }
      }

      double color[3];
      color[0] = 0;
      color[1] = 0;
      color[2] = 0;

      for (int i = 0; lights[i] != NULL; i++) {
        double RoNew[3] = {
          closestT * Rd[0] + Ro[0],
          closestT * Rd[1] + Ro[1],
          closestT * Rd[2] + Ro[2]
        };
        double RdNew[3] = {
          lights[i]->position[0] - RoNew[0],
          lights[i]->position[1] - RoNew[1],
          lights[i]->position[2] - RoNew[2]
        };

        int shadow = 0;
        for (int j = 0; objects[j] != NULL; j++) {
          double t = 0;
          if (objects[j] == closestObject) continue;
          switch(objects[j]->kind) {
            case PLANE:
              t = planeIntersection(Ro, Rd,
                objects[j]->position,
                objects[j]->plane.normal);
              break;
            case SPHERE:
              t = sphereIntersection(Ro, Rd,
                objects[j]->position,
                objects[j]->sphere.radius);
              break;
            default:
              fprintf(stderr, "Error: Object does not have an appropriate kind.");
              exit(1);
          }
          if (t > 0 && t < magnitude(RdNew)) {
            shadow = 1;
            break;
          }

          double* N = malloc(sizeof(double) * 3);
          if (closestObject->kind == PLANE) {
            N = closestObject->plane.normal;
          } else if (closestObject->kind == SPHERE) {
            N[0] = RoNew[0] - closestObject->position[0];
            N[1] = RoNew[1] - closestObject->position[1];
            N[2] = RoNew[2] - closestObject->position[2];
          }

          double* L = RdNew;

          double* D = Rd;
        }
      }

      if (closestObject != NULL) {
        pixmap[(M - 1) * N - (y * N) + x].r = (unsigned char)(clamp(closestObject->color[0], 0, 1) * MAX_COLOR_VALUE);
        pixmap[(M - 1) * N - (y * N) + x].g = (unsigned char)(clamp(closestObject->color[1], 0, 1) * MAX_COLOR_VALUE);
        pixmap[(M - 1) * N - (y * N) + x].b = (unsigned char)(clamp(closestObject->color[2], 0, 1) * MAX_COLOR_VALUE);
      } else {
        pixmap[(M - 1) * N - (y * N) + x].r = 0;
        pixmap[(M - 1) * N - (y * N) + x].g = 0;
        pixmap[(M - 1) * N - (y * N) + x].b = 0;
      }
    }
  }
}

void writeP6(char* outputPath, int width, int height) {
  FILE* fh = fopen(outputPath, "wb");
  if (fh == NULL) {
    fprintf(stderr, "Error: Output file not found.\n");
  }
  fprintf(fh, "P6\n# Converted with Robert Rasmussen's ppmrw\n%d %d\n%d\n", width, height, MAX_COLOR_VALUE);
  fwrite(pixmap, sizeof(Pixel), width*height, fh);
  fclose(fh);
}

void displayObjects() {
  printf("Camera:\n\tWidth: %lf\n\tHeight: %lf\n", camera[0]->width, camera[0]->height);
  int i = 0;
  while (objects[i] != NULL) {
    if (objects[i]->kind == PLANE) {
      printf("Plane:\n\tColor.r: %lf\n\tColor.g: %lf\n\tColor.b: %lf\n", objects[i]->color[0], objects[i]->color[1], objects[i]->color[2]);
      printf("\tPosition.x: %lf\n\tPosition.y: %lf\n\tPosition.z: %lf\n", objects[i]->position[0], objects[i]->position[1], objects[i]->position[2]);
      printf("\tNormal.x: %lf\n\tNormal.y: %lf\n\tNormal.z: %lf\n", objects[i]->plane.normal[0], objects[i]->plane.normal[1], objects[i]->plane.normal[2]);
    } else if (objects[i]->kind == SPHERE) {
      printf("Sphere:\n\tColor.r: %lf\n\tColor.g: %lf\n\tColor.b: %lf\n", objects[i]->color[0], objects[i]->color[1], objects[i]->color[2]);
      printf("\tPosition.x: %lf\n\tPosition.y: %lf\n\tPosition.z: %lf\n", objects[i]->position[0], objects[i]->position[1], objects[i]->position[2]);
      printf("\tRadius: %lf\n", objects[i]->sphere.radius);
    }
    i++;
  }
}

int main(int argc, char* argv[]) {
  if (argc != 5) {
    fprintf(stderr, "Usage: raycast width height input.json output.ppm");
    exit(1);
  }

  int width = atoi(argv[1]);
  if (width <= 0) {
    fprintf(stderr, "Error: Width must be greater than 0.");
    exit(1);
  }
  int height = atoi(argv[2]);
  if (height <= 0) {
    fprintf(stderr, "Error: Width must be greater than 0.");
    exit(1);
  }

  pixmap = malloc(sizeof(Pixel) * width * height);
  camera = malloc(sizeof(Camera));
  objects = malloc(sizeof(Object*) * 129);
  lights = malloc(sizeof(Object*) * 129);

  parseJSON(argv[3]);
  createScene(width, height);

  writeP6(argv[4], width, height);

#ifdef DEBUG
  displayObjects();
#endif


  return 0;
}
