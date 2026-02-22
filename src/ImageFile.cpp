#ifdef PLATFORM_MAC
// Mac implementation of ImageFile using stb_image + OpenGL textures for display
// stb implementations defined here (before ImageFile.h pulls the headers)
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "ImageFile.h"
#include <OpenGL/gl.h>
#include <cstring>
#include <cstdlib>

ImageFile::ImageFile() : data(nullptr), width(0), height(0), channels(0), texture_loaded(false), image_texture(nullptr) {}

ImageFile::~ImageFile() {
    if (texture_loaded && image_texture) {
        GLuint tex = (GLuint)(uintptr_t)image_texture;
        glDeleteTextures(1, &tex);
    }
    if (data) stbi_image_free(data);
}

bool ImageFile::FromFile(std::string filename) {
    if (data) { stbi_image_free(data); data = nullptr; }
    texture_loaded = false;
    if (image_texture) {
        GLuint tex = (GLuint)(uintptr_t)image_texture;
        glDeleteTextures(1, &tex);
        image_texture = nullptr;
    }
    data = stbi_load(filename.c_str(), &width, &height, &channels, 4);
    channels = 4;
    return data != nullptr;
}

void ImageFile::FromBytes(uint8_t* bytes, int data_len, int w, int h) {
    if (data) { stbi_image_free(data); data = nullptr; }
    texture_loaded = false;
    if (image_texture) {
        GLuint tex = (GLuint)(uintptr_t)image_texture;
        glDeleteTextures(1, &tex);
        image_texture = nullptr;
    }
    width = w; height = h; channels = 4;
    data = (uint8_t*)malloc(data_len);
    if (data) memcpy(data, bytes, data_len);
}

int ImageFile::getWidth()  { return width; }
int ImageFile::getHeight() { return height; }

uint8_t* ImageFile::getData() {
    uint8_t* pCopy = new uint8_t[width * height * channels];
    memcpy(pCopy, data, width * height * channels);
    return pCopy;
}

uint8_t* ImageFile::resizeAndGetData(int desired_width, int desired_height) {
    if (!data) return nullptr;
    uint8_t* pCopy = new uint8_t[desired_width * desired_height * 4];
    uint8_t* iCopy = new uint8_t[width * height * 4];
    memcpy(iCopy, data, width * height * 4);
    stbir_resize_uint8(iCopy, width, height, 0, pCopy, desired_width, desired_height, 0, 4);
    delete[] iCopy;
    return pCopy;
}

uint8_t* ImageFile::resizeAndGetDataWithFilter(int desired_width, int desired_height, stbir_filter filter) {
    if (!data) return nullptr;
    uint8_t* out = new uint8_t[desired_width * desired_height * 4];
    stbir_resize_uint8_generic(data, width, height, 0, out, desired_width, desired_height, 0,
        4, -1, 0, STBIR_EDGE_CLAMP, filter, STBIR_COLORSPACE_SRGB, nullptr);
    return out;
}

bool ImageFile::LoadTexture(ID3D11Device* /*g_pd3dDevice*/, ID3D11ShaderResourceView** out_srv) {
    if (!data) return false;
    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    *out_srv = (ID3D11ShaderResourceView*)(uintptr_t)tex;
    return true;
}

void ImageFile::imgui_Display(ID3D11Device* g_pd3dDevice) {
    if (!texture_loaded) {
        bool ret = LoadTexture(g_pd3dDevice, &image_texture);
        IM_ASSERT(ret);
        texture_loaded = true;
    }
    ImGui::Image((void*)(uintptr_t)image_texture, ImVec2(256, 256));
}
#endif // PLATFORM_MAC
