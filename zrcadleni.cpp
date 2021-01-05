#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <cassert>
#include <cmath>
#include <cctype>
#include <climits>

#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <vector>
#include <map>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <list>
#include <algorithm>
#include <functional>
#include <memory>
using namespace std;

const uint16_t ENDIAN_LITTLE = 0x4949;
const uint16_t ENDIAN_BIG    = 0x4d4d;


struct T_hdr
{
    uint16_t endian,
             width,
             height,
             format;

    bool endianness ();
    bool unset_check () const;
    int channels () const;
    int bits () const;
    friend bool operator== ( T_hdr, T_hdr );
};

struct Pixel
{
    uint8_t * imageData;
};

struct T_img
{
    T_hdr hdr;
    uint width,
         height,
         bytes_per_pixel;
    Pixel ** pixelData;

    bool Read ( const char * srcFileName );
    bool Write ( const char * dstFileName );
    void HorizontalFlip ();
    void VerticalFlip ();
    void DeallocateMemory ();
    friend bool operator== ( T_img, T_img );
};

bool T_hdr::endianness ()
{
    if ( endian == ENDIAN_LITTLE )
        return true;
    else if ( endian == ENDIAN_BIG )
    {
        width = (width >> 8) | (width << 8);
        height = (height >> 8) | (height << 8);
        format = (format >> 8) | (format << 8);
        return true;
    }
    else
        return false;
}

bool T_hdr::unset_check () const
{
    uint16_t unset_mask = ~0;
    unset_mask >>= 5;
    uint16_t unset_bits = format >> 5;

    if ( (unset_bits & unset_mask) == 0 )
        return true;
    else
        return false;
}

int T_hdr::channels () const
{
    const uint16_t channels_mask = 0x3; // 0b11
    uint16_t channels_cnt = format & channels_mask;

    if ( channels_cnt == 0 )
        return 1;
    else if ( channels_cnt == 2 )
        return 3;
    else if ( channels_cnt == 3 )
        return 4;
    else
        return 0;
}

int T_hdr::bits () const
{
    const uint16_t bits_mask = 0x7; // 0b11100 >> 2 = 0b111
    uint16_t bits_cnt = (format >> 2) & bits_mask;

    if ( bits_cnt == 0 )
        return 1; // 1
    else if ( bits_cnt == 3 )
        return 8;
    else if ( bits_cnt == 4 )
        return 16;
    else
        return 0;
}

bool T_img::Read ( const char * srcFileName )
{
    ifstream input ( srcFileName, ios::binary | ios::in );

    if ( ! input . read ( (char *) & hdr, sizeof(hdr) ) )
        return false;

    // endianness
    if ( ! hdr . endianness () )
        return false;

    // setting width, height
    width = hdr . width;
    height = hdr . height;

    if ( ! width || ! height )
        return false;

    // check for unset bits
    if ( ! hdr . unset_check () )
        return false;

    // channels per pixel
    int channels_cnt = hdr . channels ();
    if ( ! channels_cnt )
        return false;

    // bits per channel
    int bits_cnt = hdr . bits ();
    if ( ! bits_cnt )
        return false;

    // count of bytes per pixel
    bytes_per_pixel = channels_cnt * ( bits_cnt >> 3 );

    // get position of the current char
    uint curr = input . tellg ();

    // get length of a file
    input . seekg ( 0, input . end );
    uint length = input . tellg ();

    // check for the right length
    const int head_size = 8;
    if ( ( length - head_size ) != width * height * bytes_per_pixel )
        return false;

    // set back the position for reading from a file
    input . seekg ( curr );

    // create a 3D array of rows * pixels (columns) * channels
    pixelData = new Pixel * [ height ];
    for ( size_t i = 0; i < height; i++ ) {
        pixelData[ i ] = new Pixel [ width ];
        for ( size_t j = 0; j < width; j++ ) {
            pixelData[ i ][ j ] . imageData = new uint8_t [ bytes_per_pixel ];
        }
    }

    // read pixels to pixelData
    for ( size_t i = 0; i < height; i++ ) {
        for ( size_t j = 0; j < width; j++ ) {
            for ( size_t k = 0; k < bytes_per_pixel; k++ ) {
                if ( ! input . read ( (char *) & pixelData[i][j] . imageData[k],
                                      sizeof(pixelData[i][j] . imageData[k]) ) )
                {
                    DeallocateMemory ();
                    return false;
                }
            }
        }
    }

    return true;
}

bool T_img::Write ( const char * dstFileName )
{
    ofstream output ( dstFileName, ios::binary | ios::out );

    // if bytes order is big-endian, the reformat is needed
    if ( hdr . endian == ENDIAN_BIG )
    {
        hdr . width = (hdr . width >> 8) | (hdr . width << 8);
        hdr . height = (hdr . height >> 8) | (hdr . height << 8);
        hdr . format = (hdr . format >> 8) | (hdr . format << 8);
    }

    if ( ! output . write ( (const char *) & hdr, sizeof(hdr) ) )
        return false;

    for ( size_t i = 0; i < height; i++ ) {
        for ( size_t j = 0; j < width; j++ ) {
            if ( ! output . write ( (const char *) pixelData[i][j] . imageData,
                                    bytes_per_pixel * sizeof(*pixelData[i][j] . imageData) ) )
                return false;
        }
    }

    return true;
}

void T_img::HorizontalFlip ()
{
    uint8_t * swap;
    for ( size_t i = 0; i < (width/2); i++ ) {
        for ( size_t j = 0; j < height; j++ ) {
            swap = pixelData[ j ][ i ] . imageData;
            pixelData[ j ][ i ] . imageData = pixelData[ j ][ width - 1 - i ] . imageData;
            pixelData[ j ][ width - 1 - i ] . imageData = swap;
        }
    }
}

void T_img::VerticalFlip ()
{
    Pixel * swap;
    for ( size_t i = 0; i < (height/2); i++ ) {
        swap = pixelData[ i ];
        pixelData[ i ] = pixelData[ height - 1 - i ];
        pixelData[ height - 1 - i ] = swap;
    }
}

void T_img::DeallocateMemory ()
{
    for ( size_t i = 0; i < height; i++ ) {
        for ( size_t j = 0; j < width; j++ ) {
            delete [] pixelData[ i ][ j ] . imageData;
        }
        delete [] pixelData[ i ];
    }
    delete [] pixelData;
}

bool flipImage ( const char  * srcFileName,
                 const char  * dstFileName,
                 bool          flipHorizontal,
                 bool          flipVertical )
{
    T_img image;

    if ( ! image . Read ( srcFileName ) )
        return false;

    if ( flipHorizontal )
        image . HorizontalFlip ();
    if ( flipVertical )
        image . VerticalFlip ();

    if ( ! image . Write ( dstFileName ) )
        return false;

    image . DeallocateMemory ();

    return true;
}

bool operator== ( T_hdr hdr1, T_hdr hdr2 )
{
    return hdr1 . endian == hdr2 . endian &&
           hdr1 . width == hdr2 . width &&
           hdr1 . height == hdr2 . height &&
           hdr1 . format == hdr2 . format;
}

bool operator== ( T_img img1, T_img img2 )
{
    for ( size_t i = 0; i < img1 . hdr . height; i++ ) {
        for ( size_t j = 0; j < img1 . hdr . width; j++ ) {
            for ( size_t k = 0; k < img1 . bytes_per_pixel; k++ ) {
                if ( img1 . pixelData[i][j] . imageData[k] != img2 . pixelData[i][j] . imageData[k] )
                    return false;
            }
        }
    }
    return true;
}

bool identicalFiles ( const char * fileName1,
                      const char * fileName2 )
{
    T_img file1, file2;
    if ( ! file1 . Read ( fileName1 ) || ! file2 . Read ( fileName2 ) )
        return false;

    if ( ! ( file1.hdr == file1.hdr ) )
    {
        file1 . DeallocateMemory ();
        file2 . DeallocateMemory ();
        return false;
    }
    // comparing pixel
    if ( ! ( file1 == file1 ) )
    {
        file1 . DeallocateMemory ();
        file2 . DeallocateMemory ();
        return false;
    }

    file1 . DeallocateMemory ();
    file2 . DeallocateMemory ();
    return true;
}

int main ( void )
{
    assert ( flipImage ( "testImages/input_00.img", "testImages/output_00.img", true, false )
             && identicalFiles ( "testImages/output_00.img", "testImages/ref_00.img" ) );

    assert ( flipImage ( "testImages/input_01.img", "testImages/output_01.img", false, true )
             && identicalFiles ( "testImages/output_01.img", "testImages/ref_01.img" ) );

    assert ( flipImage ( "testImages/input_02.img", "testImages/output_02.img", true, true )
             && identicalFiles ( "testImages/output_02.img", "testImages/ref_02.img" ) );

    assert ( flipImage ( "testImages/input_03.img", "testImages/output_03.img", false, false )
             && identicalFiles ( "testImages/output_03.img", "testImages/ref_03.img" ) );

    assert ( flipImage ( "testImages/input_04.img", "testImages/output_04.img", true, false )
             && identicalFiles ( "testImages/output_04.img", "testImages/ref_04.img" ) );

    assert ( flipImage ( "testImages/input_05.img", "testImages/output_05.img", true, true )
             && identicalFiles ( "testImages/output_05.img", "testImages/ref_05.img" ) );

    assert ( flipImage ( "testImages/input_06.img", "testImages/output_06.img", false, true )
             && identicalFiles ( "testImages/output_06.img", "testImages/ref_06.img" ) );

    assert ( flipImage ( "testImages/input_07.img", "testImages/output_07.img", true, false )
             && identicalFiles ( "testImages/output_07.img", "testImages/ref_07.img" ) );

    assert ( flipImage ( "testImages/input_08.img", "testImages/output_08.img", true, true )
             && identicalFiles ( "testImages/output_08.img", "testImages/ref_08.img" ) );

    assert ( ! flipImage ( "testImages/input_09.img", "testImages/output_09.img", true, false ) );

    // extra inputs (optional & bonus tests)
    assert ( flipImage ( "testImages/extra_input_00.img", "testImages/extra_out_00.img", true, false )
             && identicalFiles ( "testImages/extra_out_00.img", "testImages/extra_ref_00.img" ) );
    assert ( flipImage ( "testImages/extra_input_01.img", "testImages/extra_out_01.img", false, true )
             && identicalFiles ( "testImages/extra_out_01.img", "testImages/extra_ref_01.img" ) );
    assert ( flipImage ( "testImages/extra_input_02.img", "testImages/extra_out_02.img", true, false )
             && identicalFiles ( "testImages/extra_out_02.img", "testImages/extra_ref_02.img" ) );
    assert ( flipImage ( "testImages/extra_input_03.img", "testImages/extra_out_03.img", false, true )
             && identicalFiles ( "testImages/extra_out_03.img", "testImages/extra_ref_03.img" ) );
    assert ( flipImage ( "testImages/extra_input_04.img", "testImages/extra_out_04.img", true, false )
             && identicalFiles ( "testImages/extra_out_04.img", "testImages/extra_ref_04.img" ) );
    assert ( flipImage ( "testImages/extra_input_05.img", "testImages/extra_out_05.img", false, true )
             && identicalFiles ( "testImages/extra_out_05.img", "testImages/extra_ref_05.img" ) );
    assert ( flipImage ( "testImages/extra_input_06.img", "testImages/extra_out_06.img", true, false )
             && identicalFiles ( "testImages/extra_out_06.img", "testImages/extra_ref_06.img" ) );
    assert ( flipImage ( "testImages/extra_input_07.img", "testImages/extra_out_07.img", false, true )
             && identicalFiles ( "testImages/extra_out_07.img", "testImages/extra_ref_07.img" ) );
    // assert ( flipImage ( "testImages/extra_input_08.img", "testImages/extra_out_08.img", true, false )
    //          && identicalFiles ( "testImages/extra_out_08.img", "testImages/extra_ref_08.img" ) );
    // assert ( flipImage ( "testImages/extra_input_09.img", "testImages/extra_out_09.img", false, true )
    //          && identicalFiles ( "testImages/extra_out_09.img", "testImages/extra_ref_09.img" ) );
    // assert ( flipImage ( "testImages/extra_input_10.img", "testImages/extra_out_10.img", true, false )
    //          && identicalFiles ( "testImages/extra_out_10.img", "testImages/extra_ref_10.img" ) );
    // assert ( flipImage ( "testImages/extra_input_11.img", "testImages/extra_out_11.img", false, true )
    //          && identicalFiles ( "testImages/extra_out_11.img", "testImages/extra_ref_11.img" ) );
    return 0;
}
