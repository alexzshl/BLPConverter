#include "blp.h"
#include "blp_internal.h"
#include <squish.h>
#include <FreeImage.h>
#include <iostream>
#include <string.h>
#include <memory.h>

bool DEBUG = false;

// Forward declaration of "internal" functions
tBGRAPixel* blp1_convert_jpeg(uint8_t* pSrc, tBLP1Infos* pInfos, uint32_t size);
tBGRAPixel* blp1_convert_paletted_alpha(uint8_t* pSrc, tBLP1Infos* pInfos, unsigned int width, unsigned int height);
tBGRAPixel* blp1_convert_paletted_no_alpha(uint8_t* pSrc, tBLP1Infos* pInfos, unsigned int width, unsigned int height);
tBGRAPixel* blp1_convert_paletted_separated_alpha(uint8_t* pSrc, tBLP1Infos* pInfos, unsigned int width, unsigned int height);
tBGRAPixel* blp2_convert_paletted_no_alpha(uint8_t* pSrc, tBLP2Header* pHeader, unsigned int width, unsigned int height);
tBGRAPixel* blp2_convert_paletted_alpha1(uint8_t* pSrc, tBLP2Header* pHeader, unsigned int width, unsigned int height);
tBGRAPixel* blp2_convert_paletted_alpha4(uint8_t* pSrc, tBLP2Header* pHeader, unsigned int width, unsigned int height);
tBGRAPixel* blp2_convert_paletted_alpha8(uint8_t* pSrc, tBLP2Header* pHeader, unsigned int width, unsigned int height);
tBGRAPixel* blp2_convert_raw_bgra(uint8_t* pSrc, tBLP2Header* pHeader, unsigned int width, unsigned int height);
tBGRAPixel* blp2_convert_dxt(uint8_t* pSrc, tBLP2Header* pHeader, unsigned int width, unsigned int height, int flags);


tBLPInfos blp_processFile(FILE* pFile)
{
    tInternalBLPInfos* pBLPInfos = new tInternalBLPInfos();
    char magic[4];

    fseek(pFile, 0, SEEK_SET);
    fread((void*) magic, sizeof(uint8_t), 4, pFile);

    if (strncmp(magic, "BLP2", 4) == 0)
    {
        pBLPInfos->version = 2;

        fseek(pFile, 0, SEEK_SET);
        fread((void*) &pBLPInfos->blp2, sizeof(tBLP2Header), 1, pFile);

        pBLPInfos->blp2.nbMipLevels = 0;
        while ((pBLPInfos->blp2.offsets[pBLPInfos->blp2.nbMipLevels] != 0) && (pBLPInfos->blp2.nbMipLevels < 16))
            ++pBLPInfos->blp2.nbMipLevels;
    }
    else if (strncmp(magic, "BLP1", 4) == 0)
    {
        pBLPInfos->version = 1;

        fseek(pFile, 0, SEEK_SET);
        fread((void*) &pBLPInfos->blp1.header, sizeof(tBLP1Header), 1, pFile);

        pBLPInfos->blp1.infos.nbMipLevels = 0;
        while ((pBLPInfos->blp1.header.offsets[pBLPInfos->blp1.infos.nbMipLevels] != 0) && (pBLPInfos->blp1.infos.nbMipLevels < 16))
            ++pBLPInfos->blp1.infos.nbMipLevels;

        if (pBLPInfos->blp1.header.type == 0)
        {
            fread((void*) &pBLPInfos->blp1.infos.jpeg.headerSize, sizeof(uint32_t), 1, pFile);

            if (pBLPInfos->blp1.infos.jpeg.headerSize > 0)
            {
                pBLPInfos->blp1.infos.jpeg.header = new uint8_t[pBLPInfos->blp1.infos.jpeg.headerSize];
                fread((void*) pBLPInfos->blp1.infos.jpeg.header, sizeof(uint8_t), pBLPInfos->blp1.infos.jpeg.headerSize, pFile);
            }
            else
            {
                pBLPInfos->blp1.infos.jpeg.header = 0;
            }
        }
        else
        {
            fread((void*) &pBLPInfos->blp1.infos.palette, sizeof(pBLPInfos->blp1.infos.palette), 1, pFile);
        }
    }
    else
    {
        delete pBLPInfos;
        return 0;
    }

    return (tBLPInfos) pBLPInfos;
}


void blp_release(tBLPInfos blpInfos)
{
    tInternalBLPInfos* pBLPInfos = static_cast<tInternalBLPInfos*>(blpInfos);

    if ((pBLPInfos->version == 1) && (pBLPInfos->blp1.header.type == 0))
        delete[] pBLPInfos->blp1.infos.jpeg.header;

    delete pBLPInfos;
}


uint8_t blp_version(tBLPInfos blpInfos)
{
    tInternalBLPInfos* pBLPInfos = static_cast<tInternalBLPInfos*>(blpInfos);

    return pBLPInfos->version;
}


tBLPFormat blp_format(tBLPInfos blpInfos)
{
    tInternalBLPInfos* pBLPInfos = static_cast<tInternalBLPInfos*>(blpInfos);

    //int flag = 123;
    //std::cout << flag << std::endl;
    //std::cout << pBLPInfos->version << std::endl;

    if (pBLPInfos->version == 2)
    {
        if (pBLPInfos->blp2.type == 0)
            return BLP_FORMAT_JPEG;

        if (pBLPInfos->blp2.encoding == BLP_ENCODING_UNCOMPRESSED)
            return tBLPFormat((pBLPInfos->blp2.encoding << 16) | (pBLPInfos->blp2.alphaDepth << 8));

        if (pBLPInfos->blp2.encoding == BLP_ENCODING_UNCOMPRESSED_RAW_BGRA)
            return tBLPFormat((pBLPInfos->blp2.encoding << 16));

        return tBLPFormat((pBLPInfos->blp2.encoding << 16) | (pBLPInfos->blp2.alphaDepth << 8) | pBLPInfos->blp2.alphaEncoding);
    }
    else
    {
        //std::cout << pBLPInfos->blp1.header.type << std::endl;
        //std::cout << pBLPInfos->blp1.header.flags << std::endl;
        if (pBLPInfos->blp1.header.type == 0)
           return BLP_FORMAT_JPEG;

        if ((pBLPInfos->blp1.header.flags & 0x8) != 0)
            return BLP_FORMAT_PALETTED_ALPHA_8;

        return BLP_FORMAT_PALETTED_NO_ALPHA;
    }
}


unsigned int blp_width(tBLPInfos blpInfos, unsigned int mipLevel)
{
    tInternalBLPInfos* pBLPInfos = static_cast<tInternalBLPInfos*>(blpInfos);

    if (pBLPInfos->version == 2)
    {
        // Check the mip level
        if (mipLevel >= pBLPInfos->blp2.nbMipLevels)
            mipLevel = pBLPInfos->blp2.nbMipLevels - 1;

        return (pBLPInfos->blp2.width >> mipLevel);
    }
    else
    {
        // Check the mip level
        if (mipLevel >= pBLPInfos->blp1.infos.nbMipLevels)
            mipLevel = pBLPInfos->blp1.infos.nbMipLevels - 1;

        return (pBLPInfos->blp1.header.width >> mipLevel);
    }
}


unsigned int blp_height(tBLPInfos blpInfos, unsigned int mipLevel)
{
    tInternalBLPInfos* pBLPInfos = static_cast<tInternalBLPInfos*>(blpInfos);

    if (pBLPInfos->version == 2)
    {
        // Check the mip level
        if (mipLevel >= pBLPInfos->blp2.nbMipLevels)
            mipLevel = pBLPInfos->blp2.nbMipLevels - 1;

        return (pBLPInfos->blp2.height >> mipLevel);
    }
    else
    {
        // Check the mip level
        if (mipLevel >= pBLPInfos->blp1.infos.nbMipLevels)
            mipLevel = pBLPInfos->blp1.infos.nbMipLevels - 1;

        return (pBLPInfos->blp1.header.height >> mipLevel);
    }
}


unsigned int blp_nbMipLevels(tBLPInfos blpInfos)
{
    tInternalBLPInfos* pBLPInfos = static_cast<tInternalBLPInfos*>(blpInfos);

    if (pBLPInfos->version == 2)
        return pBLPInfos->blp2.nbMipLevels;
    else
        return pBLPInfos->blp1.infos.nbMipLevels;
}


tBGRAPixel* blp_convert(FILE* pFile, tBLPInfos blpInfos, unsigned int mipLevel)
{
    tInternalBLPInfos* pBLPInfos = static_cast<tInternalBLPInfos*>(blpInfos);

    // Check the mip level
    if (pBLPInfos->version == 2)
    {
        if (mipLevel >= pBLPInfos->blp2.nbMipLevels)
            mipLevel = pBLPInfos->blp2.nbMipLevels - 1;
    }
    else
    {
        if (mipLevel >= pBLPInfos->blp1.infos.nbMipLevels)
            mipLevel = pBLPInfos->blp1.infos.nbMipLevels - 1;
    }

    // Declarations
    unsigned int width  = blp_width(pBLPInfos, mipLevel);
    unsigned int height = blp_height(pBLPInfos, mipLevel);
    tBGRAPixel* pDst    = 0;
    uint8_t* pSrc       = 0;
    uint32_t offset;
    uint32_t size;

    if (pBLPInfos->version == 2)
    {
        offset = pBLPInfos->blp2.offsets[mipLevel];
        size   = pBLPInfos->blp2.lengths[mipLevel];
    }
    else
    {
        offset = pBLPInfos->blp1.header.offsets[mipLevel];
        size   = pBLPInfos->blp1.header.lengths[mipLevel];
    }

    pSrc = new uint8_t[size];

    // Read the data from the file
    if (DEBUG) std::cout << "mipLevel: " << mipLevel << std::endl;
    if (DEBUG) std::cout << "offset: " << offset << std::endl;
    if (DEBUG) std::cout << "size: " << size << std::endl;
    if (DEBUG) std::cout << "sizeof(uint8_t): " << sizeof(uint8_t) << std::endl;
    fseek(pFile, offset, SEEK_SET);
    fread((void*) pSrc, sizeof(uint8_t), size, pFile);

    switch (blp_format(pBLPInfos))
    {
        case BLP_FORMAT_JPEG:
            //std::cout << "BLP_FORMAT_JPEG" << std::endl;
            
            // if (pBLPInfos->version == 2)
            //     pDst = blp2_convert_paletted_no_alpha(pSrc, &pBLPInfos->blp2, width, height);
            // else
                pDst = blp1_convert_jpeg(pSrc, &pBLPInfos->blp1.infos, size);
            break;

        case BLP_FORMAT_PALETTED_NO_ALPHA:
            //std::cout << "BLP_FORMAT_PALETTED_NO_ALPHA" << std::endl;
            if (pBLPInfos->version == 2)
                pDst = blp2_convert_paletted_no_alpha(pSrc, &pBLPInfos->blp2, width, height);
            else
                pDst = blp1_convert_paletted_no_alpha(pSrc, &pBLPInfos->blp1.infos, width, height);
            break;

        case BLP_FORMAT_PALETTED_ALPHA_1:  
            //std::cout << "BLP_FORMAT_PALETTED_ALPHA_1" << std::endl;
            pDst = blp2_convert_paletted_alpha1(pSrc, &pBLPInfos->blp2, width, height); break;

        case BLP_FORMAT_PALETTED_ALPHA_4:  
            //std::cout << "BLP_FORMAT_PALETTED_ALPHA_4" << std::endl;
            pDst = blp2_convert_paletted_alpha4(pSrc, &pBLPInfos->blp2, width, height); break;

        case BLP_FORMAT_PALETTED_ALPHA_8:
            //std::cout << "BLP_FORMAT_PALETTED_ALPHA_8" << std::endl;
            if (pBLPInfos->version == 2)
            {
                //std::cout << "blp2_convert_paletted_alpha8" << std::endl;
                pDst = blp2_convert_paletted_alpha8(pSrc, &pBLPInfos->blp2, width, height);
            }
            else
            {
                if (pBLPInfos->blp1.header.alphaEncoding == 5)
                {
                    //std::cout << "blp1_convert_paletted_alpha" << std::endl;
                    pDst = blp1_convert_paletted_alpha(pSrc, &pBLPInfos->blp1.infos, width, height);
                }
                else
                {
                    //std::cout << "blp1_convert_paletted_separated_alpha" << std::endl;
                    pDst = blp1_convert_paletted_separated_alpha(pSrc, &pBLPInfos->blp1.infos, width, height);
                }
            }
            break;

        case BLP_FORMAT_RAW_BGRA: pDst = blp2_convert_raw_bgra(pSrc, &pBLPInfos->blp2, width, height); break;

        case BLP_FORMAT_DXT1_NO_ALPHA:
        case BLP_FORMAT_DXT1_ALPHA_1:      pDst = blp2_convert_dxt(pSrc, &pBLPInfos->blp2, width, height, squish::kDxt1); break;
        case BLP_FORMAT_DXT3_ALPHA_4:
        case BLP_FORMAT_DXT3_ALPHA_8:      pDst = blp2_convert_dxt(pSrc, &pBLPInfos->blp2, width, height, squish::kDxt3); break;
        case BLP_FORMAT_DXT5_ALPHA_8:      pDst = blp2_convert_dxt(pSrc, &pBLPInfos->blp2, width, height, squish::kDxt5); break;
        default:                           break;
    }

    delete[] pSrc;

    return pDst;
}


std::string blp_asString(tBLPFormat format)
{
    switch (format)
    {
        case BLP_FORMAT_JPEG:              return "JPEG";
        case BLP_FORMAT_PALETTED_NO_ALPHA: return "Uncompressed paletted image, no alpha";
        case BLP_FORMAT_PALETTED_ALPHA_1:  return "Uncompressed paletted image, 1-bit alpha";
        case BLP_FORMAT_PALETTED_ALPHA_4:  return "Uncompressed paletted image, 4-bit alpha";
        case BLP_FORMAT_PALETTED_ALPHA_8:  return "Uncompressed paletted image, 8-bit alpha";
        case BLP_FORMAT_RAW_BGRA:          return "Uncompressed raw 32-bit BGRA";
        case BLP_FORMAT_DXT1_NO_ALPHA:     return "DXT1, no alpha";
        case BLP_FORMAT_DXT1_ALPHA_1:      return "DXT1, 1-bit alpha";
        case BLP_FORMAT_DXT3_ALPHA_4:      return "DXT3, 4-bit alpha";
        case BLP_FORMAT_DXT3_ALPHA_8:      return "DXT3, 8-bit alpha";
        case BLP_FORMAT_DXT5_ALPHA_8:      return "DXT5, 8-bit alpha";
        default:                           return "Unknown";
    }
}

void printInfo(const tBLP1Infos* infos) {
    std::cout << "nbMipLevels: " << infos->nbMipLevels << std::endl;
    std::cout << "jpeg.headerSize: " << infos->jpeg.headerSize << std::endl;
    // 打印其他成员的值...
}

tBGRAPixel* blp1_convert_jpeg(uint8_t* pSrc, tBLP1Infos* pInfos, uint32_t size)
{
    // pSrc 指向文件第 1180 / 0x49C 字节位置
    //std::cout << "size: " << size << std::endl;

    uint8_t* pSrcBuffer = new uint8_t[pInfos->jpeg.headerSize + size];

    memcpy(pSrcBuffer, pInfos->jpeg.header, pInfos->jpeg.headerSize);
    memcpy(pSrcBuffer + pInfos->jpeg.headerSize, pSrc, size);

    if (DEBUG) printInfo(pInfos);
    if (DEBUG) std::cout << "size: " << size << std::endl;
    if (DEBUG) std::cout << "data offset: " << pInfos->jpeg.headerSize + size << std::endl;
    FIMEMORY* pMemory = FreeImage_OpenMemory(pSrcBuffer, pInfos->jpeg.headerSize + size);

    FIBITMAP* pBitmap = FreeImage_LoadFromMemory(FIF_JPEG, pMemory);

    unsigned int width = FreeImage_GetWidth(pBitmap);
    unsigned int height = FreeImage_GetHeight(pBitmap);
    unsigned int bytespp = FreeImage_GetLine(pBitmap) / FreeImage_GetWidth(pBitmap);

    if (DEBUG) std::cout << "width: " << width << std::endl;
    if (DEBUG) std::cout << "height: " << height << std::endl;
    if (DEBUG) std::cout << "bytespp: " << bytespp << std::endl;

    tBGRAPixel* pBuffer = new tBGRAPixel[width * height];
    tBGRAPixel* pDst = pBuffer;

    uint8_t* pAlpha = pSrc + width * height;

    if (DEBUG) std::cout << "pSrc value: " << std::hex << std::uppercase << static_cast<int>(*pSrc) << std::endl;
    if (DEBUG) std::cout << "pSrc value: " << std::hex << std::uppercase << static_cast<int>(*(pSrc + 1)) << std::endl;
    if (DEBUG) std::cout << "pSrc value: " << std::hex << std::uppercase << static_cast<int>(*(pSrc + 2)) << std::endl;

    for (unsigned int y = 0; y < height; ++y)
    {
        if (DEBUG) std::cout << "y: " << y << std::endl;
        BYTE* pSrc2 = FreeImage_GetScanLine(pBitmap, height - y - 1);

        //std::cout << "pSrc2[0]: " << std::hex << std::uppercase << static_cast<int>(pSrc2[0]) << std::endl;
        //std::cout << "pSrc2[1]: " << std::hex << std::uppercase << static_cast<int>(pSrc2[1]) << std::endl;
        //std::cout << "pSrc2[2]: " << std::hex << std::uppercase << static_cast<int>(pSrc2[2]) << std::endl;
        //std::cout << "pSrc2[3]: " << std::hex << std::uppercase << static_cast<int>(pSrc2[3]) << std::endl;
        //std::cout << "pSrc2[4]: " << std::hex << std::uppercase << static_cast<int>(pSrc2[4]) << std::endl;
        //std::cout << "pSrc2[5]: " << std::hex << std::uppercase << static_cast<int>(pSrc2[5]) << std::endl;

        for (unsigned int x = 0; x < width; ++x)
        {
            //std::cout << "x: " << x << std::endl;
            //std::cout << "pSrc2 data: " << std::hex << std::uppercase << static_cast<int>(*pSrc2) << std::endl;

            // R and B are inverted in the JPEG file
            pDst->r = pSrc2[FI_RGBA_BLUE];
            //std::cout << "pSrc2[FI_RGBA_BLUE]: " << std::hex << std::uppercase << static_cast<int>(pSrc2[FI_RGBA_BLUE]) << std::endl;
            pDst->g = pSrc2[FI_RGBA_GREEN];
            //std::cout << "pSrc2[FI_RGBA_GREEN]: " << std::hex << std::uppercase << static_cast<int>(pSrc2[FI_RGBA_GREEN]) << std::endl;
            pDst->b = pSrc2[FI_RGBA_RED];
            //std::cout << "pSrc2[FI_RGBA_RED]: " << std::hex << std::uppercase << static_cast<int>(pSrc2[FI_RGBA_RED]) << std::endl;
            //std::cout << "pSrc2[FI_RGBA_ALPHA]: " << std::hex << std::uppercase << static_cast<int>(pSrc2[FI_RGBA_ALPHA]) << std::endl;
            pDst->a = 0xFF;
            //pDst->a = *pAlpha;
            //std::cout << "pAlpha value: " << std::hex << std::uppercase << static_cast<int>(*pAlpha) << std::endl;
            ++pAlpha;
            
            //pDst->a = pSrc2[FI_RGBA_ALPHA];

            //std::cout << pSrc2[FI_RGBA_ALPHA] << std::endl;
            // // 打印 pSrc2[FI_RGBA_ALPHA] 的整数值
            //int alphaValue = static_cast<int>(pSrc2[FI_RGBA_ALPHA]);
            ////std::cout << "Alpha value at (" << x << ", " << y << "): " << alphaValue << std::endl;
            //if (alphaValue == 0) {
            //    pDst->a = 0x00;
            //}
            //else {
            //    pDst->a = 0xFF;
            //}
            //pDst->a = (bytespp >= 4 && pSrc2[FI_RGBA_ALPHA] < 255) ? pSrc2[FI_RGBA_ALPHA] : 255;

            
            ++pDst;
            pSrc2 += bytespp;
            
            //if (x >= 10) {
            //    break;
            //}
            
        }

        //break;
    }

    if (DEBUG) std::cout << "pAlpha value: " << std::hex << std::uppercase << static_cast<int>(*pAlpha) << std::endl;
    ++pAlpha;
    if (DEBUG) std::cout << "pAlpha value: " << std::hex << std::uppercase << static_cast<int>(*pAlpha) << std::endl;
    

    FreeImage_Unload(pBitmap);

    FreeImage_CloseMemory(pMemory);

    return pBuffer;
}


tBGRAPixel* blp1_convert_paletted_separated_alpha(uint8_t* pSrc, tBLP1Infos* pInfos, unsigned int width, unsigned int height)
{
    tBGRAPixel* pBuffer = new tBGRAPixel[width * height];
    tBGRAPixel* pDst = pBuffer;

    uint8_t* pIndices = pSrc;
    uint8_t* pAlpha = pSrc + width * height;

    for (unsigned int y = 0; y < height; ++y)
    {
        for (unsigned int x = 0; x < width; ++x)
        {
            *pDst = pInfos->palette[*pIndices];
            pDst->a = *pAlpha;

            ++pIndices;
            ++pAlpha;
            ++pDst;
        }
    }

    return pBuffer;
}


tBGRAPixel* blp1_convert_paletted_alpha(uint8_t* pSrc, tBLP1Infos* pInfos, unsigned int width, unsigned int height)
{
    tBGRAPixel* pBuffer = new tBGRAPixel[width * height];
    tBGRAPixel* pDst = pBuffer;

    uint8_t* pIndices = pSrc;

    for (unsigned int y = 0; y < height; ++y)
    {
        for (unsigned int x = 0; x < width; ++x)
        {
            *pDst = pInfos->palette[*pIndices];
            pDst->a = 0xFF - pDst->a;

            ++pIndices;
            ++pDst;
        }
    }

    return pBuffer;
}


tBGRAPixel* blp1_convert_paletted_no_alpha(uint8_t* pSrc, tBLP1Infos* pInfos, unsigned int width, unsigned int height)
{
    tBGRAPixel* pBuffer = new tBGRAPixel[width * height];
    tBGRAPixel* pDst = pBuffer;

    uint8_t* pIndices = pSrc;

    for (unsigned int y = 0; y < height; ++y)
    {
        for (unsigned int x = 0; x < width; ++x)
        {
            *pDst = pInfos->palette[*pIndices];
            pDst->a = 0xFF;

            ++pIndices;
            ++pDst;
        }
    }

    return pBuffer;
}


tBGRAPixel* blp2_convert_paletted_no_alpha(uint8_t* pSrc, tBLP2Header* pHeader, unsigned int width, unsigned int height)
{
    tBGRAPixel* pBuffer = new tBGRAPixel[width * height];
    tBGRAPixel* pDst = pBuffer;

    for (unsigned int y = 0; y < height; ++y)
    {
        for (unsigned int x = 0; x < width; ++x)
        {
            *pDst = pHeader->palette[*pSrc];
            pDst->a = 0xFF;

            ++pSrc;
            ++pDst;
        }
    }

    return pBuffer;
}


tBGRAPixel* blp2_convert_paletted_alpha8(uint8_t* pSrc, tBLP2Header* pHeader, unsigned int width, unsigned int height)
{
    tBGRAPixel* pBuffer = new tBGRAPixel[width * height];
    tBGRAPixel* pDst = pBuffer;

    uint8_t* pIndices = pSrc;
    uint8_t* pAlpha = pSrc + width * height;

    for (unsigned int y = 0; y < height; ++y)
    {
        for (unsigned int x = 0; x < width; ++x)
        {
            *pDst = pHeader->palette[*pIndices];
            pDst->a = *pAlpha;

            ++pIndices;
            ++pAlpha;
            ++pDst;
        }
    }

    return pBuffer;
}


tBGRAPixel* blp2_convert_paletted_alpha1(uint8_t* pSrc, tBLP2Header* pHeader, unsigned int width, unsigned int height)
{
    tBGRAPixel* pBuffer = new tBGRAPixel[width * height];
    tBGRAPixel* pDst = pBuffer;

    uint8_t* pIndices = pSrc;
    uint8_t* pAlpha = pSrc + width * height;
    uint8_t counter = 0;

    for (unsigned int y = 0; y < height; ++y)
    {
        for (unsigned int x = 0; x < width; ++x)
        {
            *pDst = pHeader->palette[*pIndices];
            pDst->a = (*pAlpha & (1 << counter) ? 0xFF : 0x00);

            ++pIndices;
            ++pDst;

            ++counter;
            if (counter == 8)
            {
                ++pAlpha;
                counter = 0;
            }
        }
    }

    return pBuffer;
}

tBGRAPixel* blp2_convert_paletted_alpha4(uint8_t* pSrc, tBLP2Header* pHeader, unsigned int width, unsigned int height)
{
    tBGRAPixel* pBuffer = new tBGRAPixel[width * height];
    tBGRAPixel* pDst = pBuffer;

    uint8_t* pIndices = pSrc;
    uint8_t* pAlpha = pSrc + width * height;
    uint8_t counter = 0;

    for (unsigned int y = 0; y < height; ++y)
    {
        for (unsigned int x = 0; x < width; ++x)
        {
            *pDst = pHeader->palette[*pIndices];
            pDst->a = (*pAlpha >> counter) & 0xF;

            // convert 4-bit range to 8-bit range
            pDst->a = (pDst->a << 4) | pDst->a;

            ++pIndices;
            ++pDst;

            counter += 4;
            if (counter == 8)
            {
                ++pAlpha;
                counter = 0;
            }
        }
    }

    return pBuffer;
}

tBGRAPixel* blp2_convert_raw_bgra(uint8_t* pSrc, tBLP2Header* pHeader, unsigned int width, unsigned int height)
{
    tBGRAPixel* pBuffer = new tBGRAPixel[width * height];
    tBGRAPixel* pDst = pBuffer;

    for (unsigned int y = 0; y < height; ++y)
    {
        for (unsigned int x = 0; x < width; ++x)
        {
            pDst->b = pSrc[0];
            pDst->g = pSrc[1];
            pDst->r = pSrc[2];
            pDst->a = pSrc[3];

            pSrc += 4;
            ++pDst;
        }
    }

    return pBuffer;
}

tBGRAPixel* blp2_convert_dxt(uint8_t* pSrc, tBLP2Header* pHeader, unsigned int width, unsigned int height, int flags)
{
    squish::u8* rgba = new squish::u8[width * height * 4];
    tBGRAPixel* pBuffer = new tBGRAPixel[width * height];

    squish::u8* pSrc2 = rgba;
    tBGRAPixel* pDst = pBuffer;

    squish::DecompressImage(rgba, width, height, pSrc, flags);

    for (unsigned int y = 0; y < height; ++y)
    {
        for (unsigned int x = 0; x < width; ++x)
        {
            pDst->r = pSrc2[0];
            pDst->g = pSrc2[1];
            pDst->b = pSrc2[2];
            pDst->a = pSrc2[3];

            pSrc2 += 4;
            ++pDst;
        }
    }

    delete[] rgba;

    return pBuffer;
}
