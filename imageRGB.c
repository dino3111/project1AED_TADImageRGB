/// imageRGB - A simple image module for handling RGB images,
///            pixel color values are represented using a look-up table (LUT)
///
/// This module is part of a programming project
/// for the course AED, DETI / UA.PT
///
/// You may freely use and modify this code, at your own risk,
/// as long as you give proper credit to the original and subsequent authors.
///
/// The AED Team <jmadeira@ua.pt, jmr@ua.pt, ...>
/// 2025

// Student authors (fill in below):
// NMec: 125102
// Name: Maria Mané
// NMec: 127368
// Name: Claudino Martins
//
// Date: 28.11.2025
//

#include "imageRGB.h"

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "PixelCoords.h"
#include "PixelCoordsQueue.h"
#include "PixelCoordsStack.h"
#include "instrumentation.h"

// The data structure
//
// A RGB image is stored in a structure containing 5 fields:
// Two integers store the image width and height.
// The next field is a pointer to an array that stores the pointers
// to the image rows.
//
// Clients should use images only through variables of type Image,
// which are pointers to the image structure, and should not access the
// structure fields directly.

// FIXED SIZE of LUT for storing RGB triplets
#define FIXED_LUT_SIZE 1000

// Internal structure for storing RGB images
struct image {
  uint32 width;
  uint32 height;
  uint16** image;  // pointer to an array of pointers referencing the image rows
  uint16 num_colors;  // the number of colors (i.e., pixel labels) used
  rgb_t* LUT;         // table storing (R,G,B) triplets
};

// Design by Contract

// This module follows "design-by-contract" principles.
// `assert` is used to check function preconditions, postconditions
// and type invariants.
// This helps to find programmer errors.

/// Defensive Error Handling

// In this module, only functions dealing with memory allocation or file
// (I/O) operations use defensive techniques.
//
// When one of these functions detects a memory or I/O error,
// it immediately prints an error message and aborts the program.
// This is a Fail-Fast strategy.
//
// You may use the `check` function to check a condition
// and exit the program with an error message if it is false.
// Note that it works similarly to `assert`, but cannot be disabled.
// It should be used to detect "external" uncontrolable errors,
// and not for "internal" programmer errors.
//
// See how it's used in ImageLoadPBM, for example.

// Check a condition and if false, print failmsg and exit.
static void check(int condition, const char* failmsg) {
  if (!condition) {
    perror(failmsg);
    exit(errno || 255);
  }
}

/// Init Image library.  (Call once!)
/// Currently, simply calibrate instrumentation and set names of counters.
void ImageInit(void) {  ///
  InstrCalibrate();
  InstrName[0] = "pixmem";  // InstrCount[0] will count pixel array acesses
  // Name other counters here...
}

// Macros to simplify accessing instrumentation counters:
#define PIXMEM InstrCount[0]
// Add more macros here...

// TIP: Search for PIXMEM or InstrCount to see where it is incremented!

/// Auxiliary (static) functions

static Image AllocateImageHeader(uint32 width, uint32 height) {
  // Create the header of an image data structure
  // Allocate the array of pointers to rows
  // And the look-up table

  Image newHeader = malloc(sizeof(struct image));
  // Error handling
  check(newHeader != NULL, "malloc");

  newHeader->width = width;
  newHeader->height = height;

  // Allocating the array of pointers to image rows
  newHeader->image = malloc(height * sizeof(uint16*));
  // Error handling
  check(newHeader->image != NULL, "Alloc failed ->image array");

  // Allocating the LUT
  newHeader->LUT = malloc(FIXED_LUT_SIZE * sizeof(rgb_t));
  // Error handling
  check(newHeader->LUT != NULL, "Alloc failed ->LUT array");

  // Initialize LUT with 2 fixed colors
  newHeader->num_colors = 2;
  newHeader->LUT[0] = 0xffffff;  // RGB WHITE
  newHeader->LUT[1] = 0x000000;  // RGB BLACK

  return newHeader;
}

// Allocate row of background (label=0) pixels
static uint16* AllocateRowArray(uint32 size) {
  uint16* newArray = calloc((size_t)size, sizeof(uint16));
  // Error handling
  check(newArray != NULL, "AllocateRowArray");

  return newArray;
}

/// Find color label for given RGB color in img LUT.
/// Return the label or -1 if not found.
static int LUTFindColor(Image img, rgb_t color) {
  for (uint16 index = 0; index < img->num_colors; index++) {
    if (img->LUT[index] == color) return index;
  }
  return -1;
}

/// Return color label for RGB color in img LUT.
/// Finds existing color or allocs new one!
static int LUTAllocColor(Image img, rgb_t color) {
  int index = LUTFindColor(img, color);
  if (index < 0) {
    check(img->num_colors < FIXED_LUT_SIZE, "LUT Overflow");
    index = img->num_colors++;
    img->LUT[index] = color;
  }
  return index;
}

/// Return a pseudo-random successor of the given color.
static rgb_t GenerateNextColor(rgb_t color) {
  return (color + 7639) & 0xffffff;
}

/// Image management functions

/// Create a new RGB image. All pixels with the background WHITE color.
///   width, height: the dimensions of the new image.
/// Requires: width and height must be non-negative.
///
/// On success, a new image is returned.
/// (The caller is responsible for destroying the returned image!)
Image ImageCreate(uint32 width, uint32 height) {
  assert(width > 0);
  assert(height > 0);

  // Just two possible pixel colors
  Image img = AllocateImageHeader(width, height);

  // Creating the image rows
  for (uint32 i = 0; i < height; i++) {
    img->image[i] = AllocateRowArray(width);  // Alloc all WHITE row
  }

  return img;
}

/// Create a new RGB image, with a color chess pattern.
/// The background is WHITE.
///   width, height: the dimensions of the new image.
///   edge: the width and height of a chess square.
///   color: the foreground color.
/// Requires: width, height and edge must be non-negative.
///
/// On success, a new image is returned.
/// (The caller is responsible for destroying the returned image!)
Image ImageCreateChess(uint32 width, uint32 height, uint32 edge, rgb_t color) {
  assert(width > 0);
  assert(height > 0);
  assert(edge > 0);

  Image img = ImageCreate(width, height);

  // Alloc color in LUT.
  uint8 label = LUTAllocColor(img, color);

  // Assigning the color to each image pixel

  // Pixel (0, 0) gets the chosen color label
  for (uint32 i = 0; i < height; i++) {
    uint32 I = i / edge;
    for (uint32 j = 0; j < width; j++) {
      uint32 J = j / edge;
      img->image[i][j] = (I + J) % 2 ? 0 : label;
    }
  }

  // Return the created chess image
  return img;
}

/// Create an image with a palete of generated colors.
Image ImageCreatePalete(uint32 width, uint32 height, uint32 edge) {
  assert(width > 0);
  assert(height > 0);
  assert(edge > 0);

  Image img = ImageCreate(width, height);

  // Fill LUT with generated colors
  rgb_t color = 0x000000;
  while (img->num_colors < FIXED_LUT_SIZE) {
    color = GenerateNextColor(color);
    img->LUT[img->num_colors++] = color;
  }

  // number of tiles
  uint32 wtiles = width / edge;

  // Pixel (0, 0) gets the chosen color label
  for (uint32 i = 0; i < height; i++) {
    uint32 I = i / edge;
    for (uint32 j = 0; j < width; j++) {
      uint32 J = j / edge;
      img->image[i][j] = (I * wtiles + J) % FIXED_LUT_SIZE;
    }
  }

  return img;
}

/// Destroy the image pointed to by (*imgp).
///   imgp : address of an Image variable.
/// If (*imgp)==NULL, no operation is performed.
///
/// Ensures: (*imgp)==NULL.
void ImageDestroy(Image* imgp) {
  assert(imgp != NULL);

  Image img = *imgp;

  for (uint32 i = 0; i < img->height; i++) {
    free(img->image[i]);
  }
  free(img->image);
  free(img->LUT);
  free(img);

  *imgp = NULL;
}

/// Create a deep copy of the image pointed to by img.
///   img : address of an Image variable.
///
/// On success, a new copied image is returned.
/// (The caller is responsible for destroying the returned image!)
Image ImageCopy(const Image img) {
  assert(img != NULL);

  // 1. Criar o cabeçalho da nova imagem
  // A nova imagem terá as mesmas dimensões (width, height) da original.
  // Assume-se que AllocateImageHeader já aloca a estrutura, a matriz de ponteiros (image) e a LUT.
  Image new_img = AllocateImageHeader(img->width, img->height); 

  // 2. Copiar a Look-Up Table (LUT) e o número de cores
  new_img->num_colors = img->num_colors;
  // Usa-se memcpy para uma cópia de memória eficiente da LUT (array de rgb_t)
  memcpy(new_img->LUT, img->LUT, img->num_colors * sizeof(rgb_t)); 

  // 3. Copiar os dados dos pixels (a parte mais importante da deep copy)
  for (uint32 i = 0; i < img->height; i++) {
    // Alocar a linha de pixels da nova imagem
    // O array new_img->image[i] deve apontar para uma nova área de memória
    new_img->image[i] = AllocateRowArray(img->width); 

    // Copiar o conteúdo da linha (o array de uint16, que são os índices da LUT)
    memcpy(new_img->image[i], img->image[i], img->width * sizeof(uint16));
  }

  return new_img;
}

/// Printing on the console

/// These functions do not modify the image and never fail.

/// Output the raw RGB image (i.e., print the integer value of pixel).
void ImageRAWPrint(const Image img) {
  printf("width = %d height = %d\n", (int)img->width, (int)img->height);
  printf("num_colors = %d\n", (int)img->num_colors);
  printf("RAW image\n");

  // Print the pixel labels of each image row
  for (uint32 i = 0; i < img->height; i++) {
    for (uint32 j = 0; j < img->width; j++) {
      printf("%2d", img->image[i][j]);
    }
    // At current row end
    printf("\n");
  }

  printf("LUT:\n");
  // Print the LUT (R,G,B) values
  for (int i = 0; i < (int)img->num_colors; i++) {
    rgb_t color = img->LUT[i];
    int r = color >> 16 & 0xff;
    int g = color >> 8 & 0xff;
    int b = color & 0xff;
    printf("%3d -> (%3d,%3d,%3d)\n", i, r, g, b);
  }

  printf("\n");
}

/// PBM file operations --- For BW images

// See PBM format specification: http://netpbm.sourceforge.net/doc/pbm.html

//
static void unpackBits(int nbytes, const uint8 bytes[], uint8 raw_row[]) {
  // bitmask starts at top bit
  int offset = 0;
  uint8 mask = 1 << (7 - offset);
  while (offset < 8) {  // or (mask > 0)
    for (int b = 0; b < nbytes; b++) {
      raw_row[8 * b + offset] = (bytes[b] & mask) != 0;
    }
    mask >>= 1;
    offset++;
  }
}

static void packBits(int nbytes, uint8 bytes[], const uint8 raw_row[]) {
  // bitmask starts at top bit
  int offset = 0;
  uint8 mask = 1 << (7 - offset);
  while (offset < 8) {  // or (mask > 0)
    for (int b = 0; b < nbytes; b++) {
      if (offset == 0) bytes[b] = 0;
      bytes[b] |= raw_row[8 * b + offset] ? mask : 0;
    }
    mask >>= 1;
    offset++;
  }
}

// Match and skip 0 or more comment lines in file f.
// Comments start with a # and continue until the end-of-line, inclusive.
// Returns the number of comments skipped.
static int skipComments(FILE* f) {
  char c;
  int i = 0;
  while (fscanf(f, "#%*[^\n]%c", &c) == 1 && c == '\n') {
    i++;
  }
  return i;
}

/// Load a raw PBM file.
/// Only binary PBM files are accepted.
/// On success, a new image is returned.
/// (The caller is responsible for destroying the returned image!)
Image ImageLoadPBM(const char* filename) {  ///
  int w, h;
  char c;
  FILE* f = NULL;
  Image img = NULL;

  check((f = fopen(filename, "rb")) != NULL, "Open failed");
  // Parse PBM header
  check(fscanf(f, "P%c ", &c) == 1 && c == '4', "Invalid file format");
  skipComments(f);
  check(fscanf(f, "%d ", &w) == 1 && w >= 0, "Invalid width");
  skipComments(f);
  check(fscanf(f, "%d", &h) == 1 && h >= 0, "Invalid height");
  check(fscanf(f, "%c", &c) == 1 && isspace(c), "Whitespace expected");

  // Allocate image
  img = AllocateImageHeader((uint32)w, (uint32)h);

  // Read pixels
  int nbytes = (w + 8 - 1) / 8;  // number of bytes for each row
  // using VLAs...
  uint8 bytes[nbytes];
  uint8 raw_row[nbytes * 8];
  for (uint32 i = 0; i < img->height; i++) {
    check(fread(bytes, sizeof(uint8), nbytes, f) == (size_t)nbytes,
          "Reading pixels");
    unpackBits(nbytes, bytes, raw_row);
    img->image[i] = AllocateRowArray((uint32)w);
    for (uint32 j = 0; j < (uint32)w; j++) {
      img->image[i][j] = (uint16)raw_row[j];
    }
  }

  fclose(f);
  return img;
}

/// Save image to PBM file.
/// On success, returns nonzero.
/// On failure, a partial and invalid file may be left in the system.
int ImageSavePBM(const Image img, const char* filename) {  ///
  assert(img != NULL);
  assert(img->num_colors == 2);

  int w = (int)img->width;
  int h = (int)img->height;
  FILE* f = NULL;

  check((f = fopen(filename, "wb")) != NULL, "Open failed");
  check(fprintf(f, "P4\n%d %d\n", w, h) > 0, "Writing header failed");

  // Write pixels
  int nbytes = (w + 8 - 1) / 8;  // number of bytes for each row
  // using VLAs...
  uint8 bytes[nbytes];
  uint8 raw_row[nbytes * 8];
  for (uint32 i = 0; i < img->height; i++) {
    for (uint32 j = 0; j < img->width; j++) {
      raw_row[j] = (uint8)img->image[i][j];
    }
    // Fill padding pixels with WHITE
    memset(raw_row + w, WHITE, nbytes * 8 - w);
    packBits(nbytes, bytes, raw_row);
    check(fwrite(bytes, sizeof(uint8), nbytes, f) == (size_t)nbytes,
          "Writing pixels failed");
  }

  // Cleanup
  fclose(f);

  return 0;
}

/// PPM file operations --- For RGB images

/// Load a raw PPM file.
/// Only ASCII PPM files are accepted.
/// On success, a new image is returned.
/// (The caller is responsible for destroying the returned image!)
Image ImageLoadPPM(const char* filename) {
  assert(filename != NULL);
  int w, h;
  int levels;
  char c;
  FILE* f = NULL;

  check((f = fopen(filename, "rb")) != NULL, "Open failed");
  // Parse PPM header
  check(fscanf(f, "P%c ", &c) == 1 && c == '3', "Invalid file format");
  skipComments(f);
  check(fscanf(f, "%d ", &w) == 1 && w >= 0, "Invalid width");
  skipComments(f);
  check(fscanf(f, "%d", &h) == 1 && h >= 0, "Invalid height");
  skipComments(f);
  check(fscanf(f, "%d", &levels) == 1 && 0 <= levels && levels <= 255,
        "Invalid depth");
  check(fscanf(f, "%c", &c) == 1 && isspace(c), "Whitespace expected");

  // Allocate image
  Image img = ImageCreate((uint32)w, (uint32)h);

  // Read pixels
  for (uint32 i = 0; i < img->height; i++) {
    for (uint32 j = 0; j < img->width; j++) {
      int r, g, b;
      check(fscanf(f, "%d %d %d", &r, &g, &b) == 3 && 0 <= r && r <= levels &&
                0 <= g && g <= levels && 0 <= b && b <= levels,
            "Invalid pixel color");
      rgb_t color = r << 16 | g << 8 | b;
      uint16 index = LUTAllocColor(img, color);
      img->image[i][j] = index;
      // printf("[%u][%u]: (%d,%d,%d) -> %u (%6x)\n", i, j, r,g,b, index,
      // color);
    }
    fprintf(f, "\n");
  }

  fclose(f);
  return img;
}

/// Save image to PPM file.
/// On success, returns nonzero.
/// On failure, a partial and invalid file may be left in the system.
int ImageSavePPM(const Image img, const char* filename) {
  assert(img != NULL);

  int w = (int)img->width;
  int h = (int)img->height;
  FILE* f = NULL;

  check((f = fopen(filename, "wb")) != NULL, "Open failed");
  check(fprintf(f, "P3\n%d %d\n255\n", w, h) > 0, "Writing header failed");

  // The pixel RGB values
  for (uint32 i = 0; i < img->height; i++) {
    for (uint32 j = 0; j < img->width; j++) {
      uint16 index = img->image[i][j];
      rgb_t color = img->LUT[index];
      int r = color >> 16 & 0xff;
      int g = color >> 8 & 0xff;
      int b = color & 0xff;
      fprintf(f, "  %3d %3d %3d", r, g, b);
    }
    fprintf(f, "\n");
  }

  // Cleanup
  fclose(f);

  return 0;
}

/// Information queries

/// These functions do not modify the image and never fail.

/// Get image width
uint32 ImageWidth(const Image img) {
  assert(img != NULL);
  return img->width;
}

/// Get image height
uint32 ImageHeight(const Image img) {
  assert(img != NULL);
  return img->height;
}

/// Get number of image colors
uint16 ImageColors(const Image img) {
  assert(img != NULL);
  return img->num_colors;
}

/// Image comparison

/// These functions do not modify the images and never fail.

/// Check if img1 and img2 represent equal images.
/// NOTE: The same rgb color may correspond to different LUT labels in
/// different images!
int ImageIsEqual(const Image img1, const Image img2) {
  assert(img1 != NULL);
  assert(img2 != NULL);

  // 1. Comparar Dimensões
  if (img1->width != img2->width || img1->height != img2->height) {
    return 0; // Dimensões diferentes -> imagens diferentes
  }

  // 2. Percorrer todos os Pixels
  for (uint32 i = 0; i < img1->height; i++) {
    for (uint32 j = 0; j < img1->width; j++) {
      
      // 3. Obter o índice (label) e a cor RGB de cada pixel
      uint16 index1 = img1->image[i][j];
      uint16 index2 = img2->image[i][j];

      // PIXMEM: Contabiliza o acesso ao pixel (leitura do índice)
      PIXMEM += 2; 

      rgb_t color1 = img1->LUT[index1];
      rgb_t color2 = img2->LUT[index2];

      // 4. Comparar as cores RGB
      if (color1 != color2) {
        return 0; // Cores diferentes -> imagens diferentes (Melhor Caso se for logo aqui)
      }
    }
  }

  // Pior Caso: Só chega aqui se todas as cores forem iguais.
  return 1;
}

int ImageIsDifferent(const Image img1, const Image img2) {
  assert(img1 != NULL);
  assert(img2 != NULL);

  return !ImageIsEqual(img1, img2);
}


/// Geometric transformations

/// These functions apply geometric transformations to an image,
/// returning a new image as a result.
///
/// On success, a new image is returned.
/// (The caller is responsible for destroying the returned image!)

// --- Funções Auxiliares STATIC para Rotação de 90º CW ---

// Retorna a coordenada U (coluna) da imagem original (Fonte)
// Dado: a largura original e a coordenada V (linha) na imagem de destino
static uint32_t GetSourceU_90CW(uint32_t original_width, uint32_t v_dest) {
  // u_orig = W_orig - 1 - v_dest
  return original_width - 1 - v_dest;
}

// Retorna a coordenada V (linha) da imagem original (Fonte)
// Dado: a coordenada U (coluna) na imagem de destino
static uint32_t GetSourceV_90CW(uint32_t u_dest) {
  // v_orig = u_dest
  return u_dest;
}

// --- Funções Auxiliares STATIC para Rotação de 180º CW ---

// Retorna a coordenada U (coluna) da imagem original (Fonte)
// Dado: a largura original e a coordenada U (coluna) na imagem de destino
static uint32_t GetSourceU_180CW(uint32_t original_width, uint32_t u_dest) {
  // u_orig = W - 1 - u_dest
  return original_width - 1 - u_dest;
}

// Retorna a coordenada V (linha) da imagem original (Fonte)
// Dado: a altura original e a coordenada V (linha) na imagem de destino
static uint32_t GetSourceV_180CW(uint32_t original_height, uint32_t v_dest) {
  // v_orig = H - 1 - v_dest
  return original_height - 1 - v_dest;
}


/// Rotate 90 degrees clockwise (CW).
/// Returns a rotated version of the image.
/// Ensures: The original img is not modified.
///
/// On success, a new image is returned.
/// (The caller is responsible for destroying the returned image!)
Image ImageRotate90CW(const Image img) {
  assert(img != NULL);

  uint32_t W_orig = img->width;
  uint32_t H_orig = img->height;

  // A nova imagem terá dimensões H_orig (largura) x W_orig (altura)
  uint32_t W_dest = H_orig;
  uint32_t H_dest = W_orig;

  // 1. Criar a nova imagem com as dimensões trocadas
  Image new_img = AllocateImageHeader(W_dest, H_dest);

  // 2. Copiar a LUT (Tabela de cores não muda)
  new_img->num_colors = img->num_colors;
  memcpy(new_img->LUT, img->LUT, new_img->num_colors * sizeof(rgb_t));

  // 3. Preencher os pixels da nova imagem (percorrer o DESTINO H_orig x W_orig)
  // O loop externo itera sobre as linhas (v_dest)
  for (uint32_t v_dest = 0; v_dest < H_dest; v_dest++) { 
    // Alocar a linha da nova imagem
    new_img->image[v_dest] = AllocateRowArray(W_dest); 

    // O loop interno itera sobre as colunas (u_dest)
    for (uint32_t u_dest = 0; u_dest < W_dest; u_dest++) { 

      // 4. Usar as funções auxiliares para encontrar as coordenadas de origem
      uint32_t u_orig = GetSourceU_90CW(W_orig, v_dest); 
      uint32_t v_orig = GetSourceV_90CW(u_dest);

      // 5. Copiar o índice LUT (label) do pixel de origem para o de destino
      // new_img->image[v_dest][u_dest] = img->image[v_orig][u_orig]
      new_img->image[v_dest][u_dest] = img->image[v_orig][u_orig];
    }
  }

  return new_img;
}



/// Rotate 180 degrees clockwise (CW).
/// Returns a rotated version of the image.
/// Ensures: The original img is not modified.
///
/// On success, a new image is returned.
/// (The caller is responsible for destroying the returned image!)
Image ImageRotate180CW(const Image img) {
  assert(img != NULL);

  uint32_t W = img->width;
  uint32_t H = img->height;

  // 1. Criar a nova imagem com as mesmas dimensões (W x H)
  Image new_img = AllocateImageHeader(W, H);

  // 2. Copiar a LUT (Tabela de cores não muda)
  new_img->num_colors = img->num_colors;
  memcpy(new_img->LUT, img->LUT, new_img->num_colors * sizeof(rgb_t));

  // 3. Preencher os pixels da nova imagem (percorrer o DESTINO W x H)
  for (uint32_t v_dest = 0; v_dest < H; v_dest++) {
    // Alocar a linha da nova imagem
    new_img->image[v_dest] = AllocateRowArray(W); 

    for (uint32_t u_dest = 0; u_dest < W; u_dest++) {

      // 4. Usar as funções auxiliares para encontrar as coordenadas de origem
      uint32_t u_orig = GetSourceU_180CW(W, u_dest);
      uint32_t v_orig = GetSourceV_180CW(H, v_dest);

      // 5. Copiar o índice LUT (label) do pixel de origem para o de destino
      new_img->image[v_dest][u_dest] = img->image[v_orig][u_orig];
    }
  }

  return new_img;
}



/// Check whether pixel coords (u, v) are inside img.
/// ATTENTION
///   u : column index
///   v : row index
int ImageIsValidPixel(const Image img, int u, int v) {
  return 0 <= u && u < (int)img->width && 0 <= v && v < (int)img->height;
}

/// Region Growing

/// The following three *RegionFilling* functions perform region growing
/// using some variation of the 4-neighbors flood-filling algorithm:
///   Given the coordinates (u, v) of a seed pixel,
///   fill all similarly-colored adjacent pixels with a new color label.
///
/// All of these functions receive the same arguments:
///   img: The image to operate on (and modify).
///   u, v: the coordinates of the seed pixel.
///   label: the new color label (LUT index) to fill the region with.
///
/// And return: the number of labeled pixels.

/// Each function carries out a different version of the algorithm.


// Variável auxiliar para o flood fill: armazena a cor original da região
static uint16_t s_old_label;


// Funcao auxiliar STATIC que implementa a lógica recursiva do Flood Fill.
// Retorna o número de pixels preenchidos.
static int CheckAndFillRecursive(Image img, int u, int v, uint16_t new_label) {
  // 1. Verificações e Condições de Paragem
  
  // a) Fora dos limites da imagem
  if (!ImageIsValidPixel(img, u, v)) {
    return 0;
  }
  
  // b) O pixel não tem a cor a preencher (old_label)
  // PIXMEM: Conta o acesso para leitura do pixel
  PIXMEM += 1; 
  if (img->image[v][u] != s_old_label) {
    return 0; 
  }

  // 2. Preencher o Pixel
  img->image[v][u] = new_label;
  PIXMEM += 1; // Acesso para escrita

  // 3. Chamadas Recursivas para os 4 Vizinhos (DFS)
  int count = 1;
  
  count += CheckAndFillRecursive(img, u, v - 1, new_label); // Norte
  count += CheckAndFillRecursive(img, u, v + 1, new_label); // Sul
  count += CheckAndFillRecursive(img, u - 1, v, new_label); // Oeste
  count += CheckAndFillRecursive(img, u + 1, v, new_label); // Este
  
  return count;
}


/// Region growing using the recursive flood-filling algorithm.
int ImageRegionFillingRecursive(Image img, int u, int v, uint16 label) {
  assert(img != NULL);
  assert(ImageIsValidPixel(img, u, v));
  // O assert(label < FIXED_LUT_SIZE) deve já estar no seu código

  // 1. Obter a cor original da região
  s_old_label = img->image[v][u];
  PIXMEM += 1; // Acesso inicial para leitura

  // 2. Condição de paragem inicial
  if (s_old_label == label) {
    return 0;
  }
  
  // 3. Iniciar a recursividade
  return CheckAndFillRecursive(img, u, v, label);
}


// Funcao auxiliar STATIC para verificar vizinhos, preencher, e fazer Push na Stack.
static void ProcessNeighborForStack(Image img, int next_u, int next_v, uint16_t old_label, 
                                    uint16_t new_label, Stack* s, int* count) {
  
  // 1. Verificar se o vizinho é válido (dentro dos limites)
  if (ImageIsValidPixel(img, next_u, next_v)) {
    
    // PIXMEM: Acesso para leitura do pixel vizinho
    PIXMEM += 1; 
    
    // 2. Verificar se o vizinho ainda tem a cor a preencher (old_label)
    if (img->image[next_v][next_u] == old_label) {
      
      // 3. Preencher e fazer Push
      img->image[next_v][next_u] = new_label;
      PIXMEM += 1; // Acesso para escrita
      
      // Adicionar o píxel à Pilha para ser processado (DFS Iterativo)
      StackPush(s, PixelCoordsCreate(next_u, next_v));
      (*count)++;
    }
  }
}


/// Region growing using a STACK of pixel coordinates to
/// implement the flood-filling algorithm (DFS Iterativo).
int ImageRegionFillingWithSTACK(Image img, int u, int v, uint16 label) {
  assert(img != NULL);
  assert(ImageIsValidPixel(img, u, v));
  assert(label < FIXED_LUT_SIZE);

  uint16_t old_label = img->image[v][u];
  PIXMEM += 1; // Acesso inicial para leitura

  if (old_label == label) { return 0; }
  
  // 1. Inicializar a Pilha (Stack)
  // O tamanho inicial é apenas uma sugestão.
  Stack* s = StackCreate(img->width * img->height); 
  if (s == NULL) { /* Tratar erro */ } 

  // 2. Colocar o píxel semente e preenchê-lo
  StackPush(s, PixelCoordsCreate(u, v));
  img->image[v][u] = label; 
  PIXMEM += 1; // Acesso para escrita

  int count = 1;
  
  // 3. Loop Principal (Flood Fill Iterativo - DFS)
  while (!StackIsEmpty(s)) {
    // Retirar o pixel (o topo da Pilha)
    PixelCoords p = StackPop(s); 
    int current_u = PixelCoordsGetU(p);
    int current_v = PixelCoordsGetV(p);

    // 4. Usar a função auxiliar para processar os 4 vizinhos
    // Norte (v-1)
    ProcessNeighborForStack(img, current_u, current_v - 1, old_label, label, s, &count);
    // Sul (v+1)
    ProcessNeighborForStack(img, current_u, current_v + 1, old_label, label, s, &count);
    // Oeste (u-1)
    ProcessNeighborForStack(img, current_u - 1, current_v, old_label, label, s, &count);
    // Este (u+1)
    ProcessNeighborForStack(img, current_u + 1, current_v, old_label, label, s, &count);
  }

  // 5. Destruir a Pilha
  StackDestroy(&s); 

  return count;
}


// Funcao auxiliar STATIC para verificar vizinhos, preencher, e fazer Enqueue na Queue.
// Esta lógica é comum aos algoritmos de preenchimento por Pilha e Fila.
static void ProcessNeighborForQueue(Image img, int next_u, int next_v, uint16_t old_label, 
                                    uint16_t new_label, Queue* q, int* count) {
  
  // 1. Verificar se o vizinho é válido (dentro dos limites)
  if (ImageIsValidPixel(img, next_u, next_v)) {
    
    // PIXMEM: Acesso para leitura do pixel vizinho
    PIXMEM += 1; 
    
    // 2. Verificar se o vizinho ainda tem a cor a preencher (old_label)
    if (img->image[next_v][next_u] == old_label) {
      
      // 3. Preencher e fazer Enqueue
      img->image[next_v][next_u] = new_label;
      PIXMEM += 1; // Acesso para escrita
      
      // Adicionar o píxel à fila para ser processado (BFS)
      QueueEnqueue(q, PixelCoordsCreate(next_u, next_v));
      (*count)++;
    }
  }
}


/// Region growing using a QUEUE of pixel coordinates to
/// implement the flood-filling algorithm (BFS).
int ImageRegionFillingWithQUEUE(Image img, int u, int v, uint16 label) {
  assert(img != NULL);
  assert(ImageIsValidPixel(img, u, v));
  assert(label < FIXED_LUT_SIZE);

  uint16_t old_label = img->image[v][u];
  PIXMEM += 1; // Acesso inicial para leitura

  if (old_label == label) { return 0; }
  
  // 1. Inicializar a Fila (Queue)
  Queue* q = QueueCreate(img->width * img->height);
  if (q == NULL) { /* Tratar erro */ } 

  // 2. Colocar o píxel semente e preenchê-lo
  QueueEnqueue(q, PixelCoordsCreate(u, v));
  img->image[v][u] = label; 
  PIXMEM += 1; // Acesso para escrita

  int count = 1;
  
  // 3. Loop Principal (Flood Fill Iterativo - BFS)
  while (!QueueIsEmpty(q)) {
    // Retirar o pixel (o topo/frente da Fila)
    PixelCoords p = QueueDequeue(q); 
    int current_u = PixelCoordsGetU(p);
    int current_v = PixelCoordsGetV(p);

    // 4. Usar a função auxiliar para processar os 4 vizinhos
    // Norte (v-1)
    ProcessNeighborForQueue(img, current_u, current_v - 1, old_label, label, q, &count);
    // Sul (v+1)
    ProcessNeighborForQueue(img, current_u, current_v + 1, old_label, label, q, &count);
    // Oeste (u-1)
    ProcessNeighborForQueue(img, current_u - 1, current_v, old_label, label, q, &count);
    // Este (u+1)
    ProcessNeighborForQueue(img, current_u + 1, current_v, old_label, label, q, &count);
  }

  // 5. Destruir a Fila
  QueueDestroy(&q); 

  return count;
}

/// Image Segmentation


/// Funcao auxiliar STATIC para verificar se um pixel (u,v) é um candidato válido 
/// para iniciar uma nova região (ou seja, se tem a cor WHITE).
/// O PIXMEM é incrementado aqui.
/// Retorna 1 se for WHITE, 0 caso contrário.
static int IsNewRegionCandidate(Image img, uint32_t u, uint32_t v) {
  // NOTA: Assumimos que u e v são válidos (estão dentro dos limites),
  // pois a função é chamada pelo loop de varrimento em ImageSegmentation.

  // PIXMEM: Conta o acesso para leitura do pixel
  PIXMEM += 1; 

  // Um pixel é candidato se for a cor de fundo (WHITE)
  return img->image[v][u] == WHITE;
}


/// Label each WHITE region with a different color.
/// - WHITE (the background color) has label (LUT index) 0.
/// - Use GenerateNextColor to create the RGB color for each new region.
///
/// One of the region filling functions above is passed as the
/// last argument, using a function pointer.
///
/// Returns the number of image regions found.
/// Label each WHITE region with a different color.
/// - WHITE (the background color) has label (LUT index) 0.
/// - Use GenerateNextColor to create the RGB color for each new region.
///
/// Returns the number of image regions found.
int ImageSegmentation(Image img, FillingFunction fillFunct) {
  assert(img != NULL);
  assert(fillFunct != NULL);

  uint32_t W = img->width;
  uint32_t H = img->height;
  int regions_count = 0;

  // Percorrer todos os pixels da imagem (varrimento)
  for (uint32_t v = 0; v < H; v++) { 
    for (uint32_t u = 0; u < W; u++) {

      // Usar a função auxiliar para verificar se é o início de uma nova região
      if (IsNewRegionCandidate(img, u, v)) {
        
  // 1. Gerar o próximo RGB color e alocar um novo label na LUT
  // GenerateNextColor expects an rgb_t color as input. Use the
  // last color in the LUT as seed and then allocate it in the
  // image LUT to get the LUT index (label).
  rgb_t next_color = GenerateNextColor(img->LUT[img->num_colors - 1]);
  uint16_t new_label = (uint16_t)LUTAllocColor(img, next_color);
        
        // 2. Chamar a função de preenchimento (Recursiva, Pilha ou Fila)
        int filled_count = fillFunct(img, u, v, new_label);
        
        // 3. Se a região foi preenchida, incrementar a contagem de regiões
        if (filled_count > 0) {
          regions_count++;
        }
      }
      // Se não for candidato, a varrimento avança para o próximo pixel.
    }
  }

  return regions_count;
}
