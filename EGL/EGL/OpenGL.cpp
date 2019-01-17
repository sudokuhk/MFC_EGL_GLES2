//
//  OpenGL.cpp
//  mediasdk
//
//  Created by huangkun on 10/01/19.
//  Copyright © 2017-2019 Guangzhou HanZi Inc. All rights reserved.
//

#include <render/OpenGL.h>
#include <stdio.h>
#include <stdlib.h>
#include <common/log.h>

namespace mediasdk {
    namespace opengl {
        const char * TAG = "opengl";
        
        const GLuint gl_position = 0;
        const GLuint gl_coordinate = 1;
        
        const GLfloat vertices[8] = {
            -1.0f, -1.0f,
            1.0f, -1.0f,
            1.0f, 1.0f,
            -1.0f, 1.0f,
        };
        
        const GLubyte indices[6] = {
            0, 1, 2,
            2, 3, 0
        };
        
        const GLfloat coordinate[8] = {    //anticlockwise
            0.0f, 0.0f,
            1.0f, 0.0f,
            1.0f, 1.0f,
            0.0f, 1.0f,
        };
        
        const int degree[4][4] = {
            3, 2, 1, 0, //up
            0, 3, 2, 1, //left
            1, 0, 3, 2, //down
            2, 1, 0, 3, //right
        };
        
        const char *uniform_name[] = {
            "type",
            "plane_y",
            "plane_u",
            "plane_v",
        };

#define SHADER_STRING(text) #text
        const char *shader_vs = SHADER_STRING(
              attribute vec2 position;
              attribute vec2 coordinate;
              varying mediump
              vec2 texcoord;
              
              void main() {
                  texcoord = coordinate;
                  gl_Position = vec4(position, 0.0, 1.0);
              }
        );
        
        const char *shader_fs = SHADER_STRING(
            varying mediump vec2 texcoord;
            uniform sampler2D plane_y;
            uniform sampler2D plane_u;
            uniform sampler2D plane_v;
            uniform int type;
            const mediump vec3 delta = vec3(-0.0 / 255.0, -128.0 / 255.0, -128.0 / 255.0);
            const mediump vec3 matYUVRGB1 = vec3(1.0, 0.0, 1.402);
            const mediump vec3 matYUVRGB2 = vec3(1.0, -0.344, -0.714);
            const mediump vec3 matYUVRGB3 = vec3(1.0, 1.772, 0.0);

            void main() {
                if (type > 0 && type <= 4) {
                    mediump vec3 yuv;
                    mediump vec3 result;

                    yuv.x = texture2D(plane_y, texcoord).r;
                    if (type == 1/*I420*/ || type == 2/*YV12*/) {
                        yuv.y = texture2D(plane_u, texcoord).r;
                        yuv.z = texture2D(plane_v, texcoord).r;
                    } else if (type == 3) { //NV12
                        yuv.y = texture2D(plane_u, texcoord).r;
                        yuv.z = texture2D(plane_u, texcoord).a;
                    } else if (type == 4) { //NV21
                        yuv.y = texture2D(plane_u, texcoord).a;
                        yuv.z = texture2D(plane_u, texcoord).r;
                    }

                    yuv += delta;
                    result.x = dot(yuv, matYUVRGB1);
                    result.y = dot(yuv, matYUVRGB2);
                    result.z = dot(yuv, matYUVRGB3);
                    gl_FragColor = vec4(result.rgb, 1.0);
                } else if (type == 5) {
                    gl_FragColor = vec4(0.0, 0.0, 0.0, 1.0);
                }
            }
        );
#undef SHADER_STRING
        
#define DELETE_OPENGL(n, gl, method)    \
    do {                                \
        if ((gl) > 0) {                 \
            method(n, &(gl));           \
            (gl) = 0;                   \
        }                               \
    } while (0)
        
        void deleteBuffer(GLuint &gl) {
            DELETE_OPENGL(1, gl, glDeleteBuffers);
        }
        
        void deleteTextures(GLuint &gl) {
            DELETE_OPENGL(1, gl, glDeleteTextures);
        }
        
        void deleteProgram(GLuint & gl) {
            if (gl > 0) {
                glDeleteProgram(gl);
                gl = 0;
            }
        }
        
        void deleteFramebuffers(GLuint & gl)
        {
            DELETE_OPENGL(1, gl, glDeleteFramebuffers);
        }
        
        void deleteRenderbuffers(GLuint & gl)
        {
            DELETE_OPENGL(1, gl, glDeleteRenderbuffers);
        }
#undef DELETE_OPENGL
        
        static void deleteShader(GLuint &shader) {
            if (shader > 0) {
                glDeleteShader(shader);
                shader = 0;
            }
        }
        
        static GLuint createShader(const char *sz_shader, int type) {
            GLuint shader = 0;
            GLint success = 0;
            
            shader = glCreateShader(type);
            glShaderSource(shader, 1, &sz_shader, NULL);
            glCompileShader(shader);
            glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
            if (success != GL_TRUE) {
                GLint infoLen = 0;
                glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLen);
                if (infoLen > 0) {
                    char *buf = (char *) malloc(infoLen);
                    if (buf != NULL) {
                        glGetShaderInfoLog(shader, infoLen, NULL, buf);
                        LOGE(TAG, "Could not compile shader %s:%s\n",
                             type == GL_VERTEX_SHADER ? "vertex" : "fragment", buf);
                        free(buf);
                    }
                    deleteShader(shader);
                }
            }
            return shader;
        }
        
        bool linkProgram(GLuint program) {
            if (program == 0) {
                return false;
            }
            
            GLint success = 0;
            glLinkProgram(program);
            glGetProgramiv(program, GL_LINK_STATUS, &success);
            if (success != GL_TRUE) {
                char log[1024];
                glGetProgramInfoLog(program, 1024, NULL, log);
                LOGE(TAG, "link program:%s\n", log);
                return false;
            }
            return true;
        }
        
        bool useProgram(GLuint program) {
            glUseProgram(program);            
            return true;
        }
        
        static void addAttribute(GLuint program, const char *attr, GLuint index) {
            glBindAttribLocation(program, index, attr);
        }
        
        GLuint buildProgram(void) {
            bool ok = false;
            GLuint program = 0, vs_shader = 0, fs_shader = 0;
            vs_shader = createShader(shader_vs, GL_VERTEX_SHADER);
            if (vs_shader == 0) {
                goto out;
            }
            fs_shader = createShader(shader_fs, GL_FRAGMENT_SHADER);
            if (fs_shader == 0) {
                goto out;
            }
            
            program = glCreateProgram();
            if (program == 0) {
                goto out;
            }
            glAttachShader(program, vs_shader);
            glAttachShader(program, fs_shader);
            addAttribute(program, "position", gl_position);
            addAttribute(program, "coordinate", gl_coordinate);
            
            ok = true;
        out:
            deleteShader(vs_shader);
            deleteShader(fs_shader);
            if (!ok) {
                deleteProgram(program);
            }
            return program;
        }
        
        void setUniform(GLuint program, enUniform uniform, int value) {
            if (program > 0 && uniform >= U_TYPE && uniform < U_MAX) {
                GLint idx = glGetUniformLocation(program, uniform_name[uniform]);
                if (idx >= 0) {
                    glUniform1i(idx, (GLint) value);
                }
            }
        }

        enShaderMode setShaderMode(GLuint gl, enImageFormat format) {
            enShaderMode mode = enUnknown;
            switch (format) {
                case enImageYUV420P:
                    mode = enI420;
                    break;
                case enImageYV12:
                    mode = enYV12;
                    break;
                case enImageNV12:
                    mode = enNV12;
                    break;
                case enImageNV21:
                    mode = enNV21;
                    break;
                default:
                    mode = enBLACK;
                    break;
            }
            setUniform(gl, U_TYPE, (int)mode);
            return mode;
        }
        
        void draw(SRect rect) {
            glClearColor(0.0, 0.0, 0.0, 1.0);
            glClear(GL_COLOR_BUFFER_BIT);
            glViewport(rect.point.x, rect.point.y,
                       rect.size.w, rect.size.h);
            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, 0);
        }

        void draw(enRenderMode mode, int pic_w, int pic_h,
                  int view_w, int view_h, enDirection direction) {
            SRect r = fixSize(mode, pic_w, pic_h,
                              view_w, view_h, direction);
            //LOGV(TAG, "fix, point:(%d, %d), size:%dx%d\n", r.point.x, r.point.y, r.size.w, r.size.h);
            draw(r);
        }

        void drawBlack(void)
        {
            //setUniform(_opengl.program, opengl::U_TYPE, (int)opengl::enBLACK);
            SRect rect = {0, 0, 1, 1};
            draw(rect);
        }
        
        GLuint genTexture(int layer) {
            GLuint gl;
            GLenum texture_id = GL_TEXTURE0 + layer;
            glActiveTexture(texture_id);
            glGenTextures(1, &gl);
            glBindTexture(GL_TEXTURE_2D, gl);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            
            return gl;
        }
        
        void bindTexture(int layer, GLenum fmt, int width, int height,
                         unsigned char * data) {
            GLenum texture_id = GL_TEXTURE0 + layer;
            GLenum type = GL_UNSIGNED_BYTE;
            
            glActiveTexture(texture_id);
            glTexImage2D(GL_TEXTURE_2D, 0, fmt, width, height, 0, fmt, type, data);
        }
        
        void bindYUV420(unsigned char *yuv, int width, int height) {
            unsigned char *y = yuv;
            unsigned char *u = yuv + width * height;
            unsigned char *v = yuv + width * height * 5 / 4;
            
            bindTexture(0, GL_LUMINANCE, width, height, y);
            bindTexture(1, GL_LUMINANCE, width / 2, height / 2, u);
            bindTexture(2, GL_LUMINANCE, width / 2, height / 2, v);
        }
        
        void bindYV12(unsigned char *yuv, int width, int height) {
            unsigned char *y = yuv;
            unsigned char *v = yuv + width * height;
            unsigned char *u = yuv + width * height * 5 / 4;
            
            bindTexture(0, GL_LUMINANCE, width, height, y);
            bindTexture(1, GL_LUMINANCE, width / 2, height / 2, u);
            bindTexture(2, GL_LUMINANCE, width / 2, height / 2, v);
        }
        
        void bindNV12(unsigned char *yuv, int width, int height) {
            unsigned char *y = yuv;
            unsigned char *uv = yuv + width * height;
            
            bindTexture(0, GL_LUMINANCE, width, height, y);
            bindTexture(1, GL_LUMINANCE_ALPHA, width / 2, height / 2, uv);
        }
        
        void bindNV21(unsigned char *yuv, int width, int height) {
            unsigned char *y = yuv;
            unsigned char *vu = yuv + width * height;
            
            bindTexture(0, GL_LUMINANCE, width, height, y);
            bindTexture(1, GL_LUMINANCE_ALPHA, width / 2, height / 2, vu);
        }

        void bindPicture(GLuint gl, enImageFormat fmt, uint8_t * pic, int w, int h)
        {
            enShaderMode mode = setShaderMode(gl, fmt);
            SDK_UNUSED(mode);
            switch (fmt) {
                case enImageYUV420P:
                    bindYUV420(pic, w, h);
                    break;
                case enImageNV12:
                    bindNV12(pic, w, h);
                    break;
                case enImageYV12:
                    bindYV12(pic, w, h);
                    break;
                case enImageNV21:
                    bindNV21(pic, w, h);
                    break;
                default:
                    break;
            }
        }
        
        void bindPicture(GLuint gl, enImageFormat fmt, 
                         uint8_t * y, uint8_t * u, uint8_t * v, int w, int h)
        {
            enShaderMode mode = setShaderMode(gl, fmt);
            SDK_UNUSED(mode);
            switch (fmt) {
                case enImageYUV420P:
                case enImageYV12:
                    bindTexture(0, GL_LUMINANCE, w, h, y);
                    bindTexture(1, GL_LUMINANCE, w / 2, h / 2, u);
                    bindTexture(2, GL_LUMINANCE, w / 2, h / 2, v);
                    break;
                default:
                    break;
            }
        }
        
        void bindPicture(GLuint gl, enImageFormat fmt, 
                         uint8_t * y, uint8_t * uv, int w, int h)
        {
            enShaderMode mode = setShaderMode(gl, fmt);
            SDK_UNUSED(mode);
            switch (fmt) {
                case enImageNV12:
                case enImageNV21:
                    bindTexture(0, GL_LUMINANCE, w, h, y);
                    bindTexture(1, GL_LUMINANCE_ALPHA, w / 2, h / 2, uv);
                    break;
                default:
                    break;
            }
        }
        
        static float f_ratio(enRenderMode mode, int pic_w, int pic_h,
                             int view_w, int view_h, enDirection direction) {
            float img_w = pic_w, img_h = pic_h;
            if (direction == LEFT || direction == RIGHT) {
                std::swap(img_w, img_h);
            }
            
            switch (mode) {
                case enRedner_4_3:
                    img_w = 4.0f;
                    img_h = 3.0f;
                    break;
                case enRender_16_9:
                    img_w = 16.0f;
                    img_h = 9.0f;
                    break;
                case enRenderFull:
                    break;
                case enRenderAuto:
                    break;
            }
#define MEDIASDK_OPENG_ABS(n)  ((n) > 0 ? (n) : -(n))
            float ratio = (img_h * view_w) / (img_w * view_h);
            if (MEDIASDK_OPENG_ABS(ratio - 1.0f) < 0.005f) {
                ratio = 1.0f;
            }
#undef MEDIASDK_OPENG_ABS
            return ratio;
        }
        
        SRect fixSize(enRenderMode mode, int pic_w, int pic_h, 
                      int view_w, int view_h, enDirection direction) {
            SRect rect = {0, 0, view_w, view_h};
            float ratio = f_ratio(mode, pic_w, pic_h, view_w, view_h, direction);
            if (ratio < 1.0) {
                rect.point.y = view_h * (1 - ratio) / 2;
                rect.size.h = view_h * ratio;
            } else if (ratio > 1.0) {
                rect.point.x = view_w * (1 - (1 / ratio)) / 2;
                rect.size.w = view_w * (1 / ratio);
            }
            
            return rect;
        }
        
        GLuint genAndBindVAO(void) {
            const GLsizei stride = 2 * sizeof(GLfloat);
            GLuint vertex;
            glGenBuffers(1, &vertex);
            glBindBuffer(GL_ARRAY_BUFFER, vertex);
            glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
            glVertexAttribPointer(gl_position, 2, GL_FLOAT, GL_FALSE, stride, NULL);
            glEnableVertexAttribArray(gl_position);
            
            return vertex;
        }
        
        GLuint genAndBindVBO(void) {
            GLuint gl;
            
            glGenBuffers(1, &gl);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gl);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), 
                         indices, GL_STATIC_DRAW);
            
            return gl;
        }
        
        GLuint genCoordinate(void) {
            const GLsizei stride = 2 * sizeof(GLfloat);
            GLuint gl;
            glGenBuffers(1, &gl);
            glBindBuffer(GL_ARRAY_BUFFER, gl);
            glBufferData(GL_ARRAY_BUFFER, sizeof(coordinate), coordinate, GL_DYNAMIC_DRAW);
            glVertexAttribPointer(gl_coordinate, 2, GL_FLOAT, GL_FALSE, stride, NULL);
            glEnableVertexAttribArray(gl_coordinate);
            
            return gl;
        }
        
        void updateCoordinate(GLuint gl, enDirection dir) {
            GLfloat coord[8];
            for (int i = 0; i < 4; i++) {
                coord[i * 2] = coordinate[degree[dir][i] * 2];
                coord[i * 2 + 1] = coordinate[degree[dir][i] * 2 + 1];
            }
            
            glBindBuffer(GL_ARRAY_BUFFER, gl);
            glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(coord), coord);
        }
        
        void printGLString(const char *name, GLenum s) {
            const char *v = (const char *) glGetString(s);
            LOGI(TAG, "GL %s = %s\n", name, v);
        }
    }
}
