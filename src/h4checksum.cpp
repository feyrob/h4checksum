
#include <iostream>
//#include <ext/rope>

//#include <map>

#include <cryptopp/sha.h>
#include <cryptopp/hex.h>
#include <cryptopp/files.h>
#include <string>

#include <Magick++.h>

#include <gflags/gflags.h>

#include "../include/h4checksum.h"

DEFINE_string(file, "", "raster graphics file to build checksum from");

using namespace std;
using namespace Magick;


string bitmap_to_hex_str(Image image){
  size_t width = image.columns();
  size_t height = image.rows();
  // use 64 bit per RGBA pixel (each channel 16 bit)
  // assert 16 bit per channel
  assert(image.depth() == 16);
  // assert Magick quantum depth of 16 bit per channel
  size_t magick_quantum_depth;
  MagickCore::GetMagickQuantumDepth(&magick_quantum_depth);
  assert(magick_quantum_depth == 16);

  size_t num_channels = 4;
  size_t bytes_per_channel = magick_quantum_depth / 8;
  size_t bytes_per_pixel = bytes_per_channel * num_channels; 

  Pixels view_test(image);
  string bitmap_hex_string;
  CryptoPP::StringSource((byte*)view_test.get(0,0,width,height),(size_t)(width*height*bytes_per_pixel),true, new CryptoPP::HexEncoder(new CryptoPP::StringSink(bitmap_hex_string),false));
  return bitmap_hex_string;
}


vector<unsigned char> to_counted_byte_vector(size_t num){
  vector<unsigned char> byte_vector;
    while(num != 0){
      byte_vector.push_back((unsigned char) (num % 256));
      num = (num >> 8);
    }
    byte_vector.push_back((unsigned char)byte_vector.size());
    reverse(byte_vector.begin(),byte_vector.end());
    // width_v now contains this array: [needed_bytes_for_width_byte, most_significant_byte, ..., least_significant_byte]
    return byte_vector;
}


string binary_to_hex(string binary_string,size_t size){
  bool pump_all = true;
  string result;
  CryptoPP::StringSource((byte*)binary_string.data(),size,pump_all, new CryptoPP::HexEncoder(new CryptoPP::StringSink(result),false));
  return result;
}


// returns a binary string of length CryptoPP::SHA256::DIGESTSIZE
string image_sha256(Image src_image, bool prepend_dimensions){

  Image work_image = src_image; // only a matte channel may be added to work_image

  size_t width = src_image.columns();
  size_t height = src_image.rows();

  // use 64 bit per RGBA pixel (each channel 16 bit)
  // assert 16 bit per channel
  assert(work_image.depth() == 16);
  // assert Magick quantum depth of 16 bit per channel
  size_t magick_quantum_depth;
  MagickCore::GetMagickQuantumDepth(&magick_quantum_depth);
  assert(magick_quantum_depth == 16);

  size_t num_channels = 4;
  size_t bytes_per_channel = magick_quantum_depth / 8;
  size_t bytes_per_pixel = bytes_per_channel * num_channels; 

  CryptoPP::SHA256 hasher;

  if(prepend_dimensions){
    vector<unsigned char> width_v = to_counted_byte_vector(width);
    vector<unsigned char> height_v = to_counted_byte_vector(height);

    vector<unsigned char> dimensions_v;
    dimensions_v.reserve(width_v.size() + height_v.size());
    dimensions_v.insert(dimensions_v.end(),width_v.begin(),width_v.end());
    dimensions_v.insert(dimensions_v.end(),height_v.begin(),height_v.end());

    hasher.Update((const byte*)&dimensions_v.front(),dimensions_v.size());

    //for(int i = 0; i < dimensions_v.size() ;i++){
      //cout << "byte[" << i << "] is '" << (int)dimensions_v[i] << "'" << endl;
    //}
  }

  // most likely I'm being stupid here but it seems to me that 
  // 1. Magick uses an opacity value of 0 for full opaque (non transparent) colors (seems strange to me)
  // 2. R, G, B, and A are stored in the order BGRA in Magick (just something else I found weird)
  // 3. during checksuming in Magicks internal 'signature' function the opacity seems to be negated (0000 becomes ffff)
  //    SHA gets the data in the following order red, green, blue, and opacity (opacity being negated into the more natural encoding of bigger value meaning less transparent)
  //
  // I diverge from the checksumming mechanism of Magic when there is no matte channel
  // I treat a non existing alpha/matte/opacity/'whatever you call it' channel just like a full opaque color (every pixel always uses 64bit in my implementation)
  //
  // as a result I have theses two cases (when called with prepend_dimensions=false)
  // cases:
  //   a) original with matte channel
  //     same result as Magick:
  //       magick_sig_image.depth(16); 
  //       magick_sig_image.signature(); 
  //   b) original without matte channel
  //     same result as Magick when doing this:
  //       magick_sig_image.depth(16); 
  //       magick_sig_image.matte(true); // sets opacity to 0x0000 which will be turned to 0xffff in 'signature'
  //       magick_sig_image.signature();
  //       // in this case I just feed opacity of 0xffff directly into my hasher
  // 
  // so my implementation (called with prepend_dimensions=false) should always return the same checksum as Magick called in this way:
  // magick_sig_image.depth(16);
  // magick_sig_image.matte(true); // doesn't change anything if there already was a matte channel
  // magick_sig_signature();

  // here some test results (most likely only usefull for me):
  //
  // aabbcc.png (has no matte channel)
  //  depth 16, Magick.sig -> 6ef5  (don't want that)
  //  depth 16, opacity transp, Magick.sig -> c9c7 (don't want that)
  //  depth 16, opacity opaque, Magick.sig -> e5e0 (don't want that)
  //  depth 16, matte true, Magick.sig -> 592e (yay !)
  //  my digest -> 592e
  // ryvb.png (has matte channel)
  //  depth 16, Magick.sig -> 3d04 (yay !)
  //  depth 16, opacity transp, Magick.sig -> ed9d (data loss in alpha channel!)
  //  depth 16, opacity opaque, Magick.sig -> b0a7 (data loss in alpha channel!)
  //  depth 16, matte true, Magick.sig -> 3d04 (doesn't change anything, yay !
  //  my digest -> 3d04
  //

  //CryptoPP::SHA256 hash;
  unsigned short* row = new unsigned short[width * 4];

  Pixels view(work_image);
  bool with_matte = work_image.matte();

  //cout << "bitmap: 01020102 " <<  bitmap_to_hex_str(work_image) << "   "; // << endl;
  ////print_hex_hash_hex("0101");

  for(ssize_t i_y = 0; i_y < height; i_y++){
    PixelPacket *row_start_pixel = view.get(0,i_y,width,1);
    PixelPacket *row_end = row_start_pixel + width;
    unsigned short* cur_channel = row;
    for(PixelPacket* cur_pixel = row_start_pixel; cur_pixel < row_end; cur_pixel++){
      cur_channel[0] = cur_pixel->red;
      cur_channel[1] = cur_pixel->green;
      cur_channel[2] = cur_pixel->blue;
      unsigned short alpha = QuantumRange; // for images without matte 'ffff' is used
      if(with_matte){
        // Magick internal opacity is negated for SHA
        alpha = QuantumRange - cur_pixel->opacity;
      }
      cur_channel[3] = alpha;
      cur_channel+=4;
    }
    hasher.Update((const byte*)row,width*bytes_per_pixel);
  }
  delete[] row;

  string hash_final;
  hash_final.resize(CryptoPP::SHA256::DIGESTSIZE);
  hasher.Final((byte*)hash_final.data());

  //cout << "x: " << binary_to_hex(hash_final,CryptoPP::SHA256::DIGESTSIZE) << endl;

  return hash_final; // binary digest
}


template <typename T> 
struct xor_op {
  T operator()( T ch1, T ch2 ) {
    return ch1 ^ ch2;
  }
};


template <typename InputIterator1, typename InputIterator2, 
          typename OutputIterator, typename Functor>
void apply( InputIterator1 begin1, InputIterator1 end1,
            InputIterator2 begin2, InputIterator2 end2,
            OutputIterator output, Functor f ) {
   //if ( (end1-begin1) != (end2-begin2) ){
     //throw std::exception(); 
   //}
   assert( (end1-begin1) == (end2-begin2) ); // assert same size

   while ( begin1 != end1 ) {
     //cout << "o: " << *output << endl;
      *output++ = f( *begin1++, *begin2++ );
   }
}

// this piece of code is not from me:
// usage:
//void string_operations( std::string str1, // by value so we can change it
     //std::string const & str2 )
//{
   //// in place modification
   //apply( str1.begin(), str1.end(), str2.begin(), str2.end(), 
          //str1.begin(), or<char>() );

   //// out of place: copy
   //std::string and_string;
   //apply( str1.begin(), str1.end(), str2.begin(), str2.end(), 
          //std::back_inserter(and_string), and<char>() );
//}


// rotation independant checksum
string image_ri_sha256(Image src_image){
  Image work_image = src_image;

  string xor_acc(CryptoPP::SHA256::DIGESTSIZE,(char)0);
  string xor_item;
  int i = 3; // three rotations are needed to get a digest for each
  do{
    bool prepend_dimensions = true;
    xor_item = image_sha256(work_image,prepend_dimensions); 
    apply(xor_acc.begin(),xor_acc.end(), 
          xor_item.begin(),xor_item.end(),
          xor_acc.begin(), xor_op<char>() );
    i--;
  }while( 
    // someone please tell me how to do this better
    (i >= 0) // condition for loop exit
    && 
    (
  		work_image.rotate(90.0),
    	true 
    )
  );
  return xor_acc;
}


// rotation and flip/mirror independant checksum
string image_rfi_sha256(Image src_image){
  Image work_image = src_image;

  string xor_acc = image_ri_sha256(work_image);
  work_image.flip();
  string xor_item = image_ri_sha256(work_image);

  // xor both digests
  apply(xor_acc.begin(),xor_acc.end(), 
        xor_item.begin(),xor_item.end(),
        xor_acc.begin(), xor_op<char>() );

  return xor_acc;
}


string hex_sha256_hex(string hex_str){
  string hash_result;
  CryptoPP::SHA256 hasher;
  CryptoPP::StringSource(
    hex_str,
    true, 
    new CryptoPP::HexDecoder(
      new CryptoPP::HashFilter(
        hasher,
        new CryptoPP::HexEncoder(
          new CryptoPP::StringSink(hash_result),
          false
        )
      )
    )
  );
  return hash_result;
}


void print_hex_hash_hex(string src_hex){
  string result_hex = hex_sha256_hex(src_hex);
  cout << "src_hex '" << src_hex << "' result_hex: '" << result_hex << "'" << endl;
}


//void show(string s){
  //cout << "X: " << s << endl;
//}


class Files {
  public:
  static map<string,Image> file_map;
  void operator() (string filename) {
    cout << "filename: " << filename << endl ;
    file_map.insert(pair<string,Image>(filename,Image(filename)));
  }
  //void print_name(){
    //cout << "myclass" << endl;
  //}
};

map<string,Image> Files::file_map = map<string,Image>();


void help_hash(pair<string,Image> name_image_pair){
    cout << "filename: " << name_image_pair.first << endl;
    Image work_image = name_image_pair.second;
    work_image.depth(16);
    string image_rfi_digest = image_rfi_sha256(work_image);
    cout << "rfi hash: " << binary_to_hex(image_rfi_digest,CryptoPP::SHA256::DIGESTSIZE) << endl;
    cout << endl; // << endl;
}


void h4_init(string c_argv0){
  InitializeMagick(c_argv0.c_str());
}


string h4_get_rfi_hex_digest_from_file(string filename){
  Image image;
  try{
    image.read(filename);
    image.depth(16);
    string image_rfi_digest = image_rfi_sha256(image);
    string hex_string = binary_to_hex(image_rfi_digest, CryptoPP::SHA256::DIGESTSIZE);
    return hex_string;
  }catch(Exception &error_){
    cout << "Caught exception: " << error_.what() << endl;
    return string("exception in get_rfi_digest_from_file");
  }
  return string("ERR - get_rfi_digest_from_file");
}


int main(int argc,char** argv){
  google::ParseCommandLineFlags(&argc, &argv, true);
  //cout << FLAGS_file << endl;
  InitializeMagick(*argv);

  cout << "h4_raster_graphic_" << h4_get_rfi_hex_digest_from_file(FLAGS_file);

  return 0;

  // everything below is only testing code

  Image src_image1;
  Image src_image2;
  try{
    // what Magick gives me:
    // 0000 0000 ffff dddd - alpha red
    // 0000 ebeb ffff 5151 - alpha yellow
    // ebeb 0000 ffff 5151 - alpha violet
    // fefe 0000 0000 0000 - opaque blue
    // blue green red opac - in image

    // I sort the colors:
    // ffff 0000 0000 dddd
    // ffff ebeb 0000 5151
    // ffff 0000 ebeb 5151
    // 0000 0000 fefe 0000
    // red green blue opac   

    // I negated opac:
    // ffff 0000 0000 2222
    // ffff ebeb 0000 aeae 
    // ffff 0000 ebeb aeae
    // 0000 0000 fefe ffff

    // adding 0102 0102 (byte counted width and height):
    //print_hex_hash_hex("0102 0102 ffff 0000 0000 2222 ffff ebeb 0000 aeae ffff 0000 ebeb aeae 0000 0000 fefe ffff");


    //src_image1.read("tests/bird.png");
    //src_image2.read("tests/bird_16bit_channels_rot.png");

    //src_image1.matte(false);
    //src_image1.depth(8);
    //src_image1.read("aabbcc.png");
    //src_image1.read("123456.png");
    //src_image1.read("ryvb.png");
    //src_image2.read("vrby.png");
    vector<string> filenames;
    filenames.push_back("./tests/bird.png");
    filenames.push_back("./tests/bird_rotated.png");
    filenames.push_back("./tests/bird_mirrored.png");
    filenames.push_back("./tests/bird_rotated_mirrored.png");
    filenames.push_back("./tests/bird_16bit_channels_rot.png");
    filenames.push_back("./tests/bird_lossy.jpeg");
    filenames.push_back("./tests/bird_lossy_rotated.jpeg");
    filenames.push_back("./tests/bird_lossy_to_lossless_rot.png");

    Files x;
    for_each(filenames.begin(),filenames.end(),x); 
    cout << "done loading images" << endl << endl;

    map<string,Image> files_copy = Files::file_map;
    for_each(files_copy.begin(),files_copy.end(),help_hash);
    return 0;

    src_image1.read("./tests/bird.png");
    src_image2.read("./tests/bird_rotated.png");

    Image work_image1 = src_image1;
    Image work_image2 = src_image2;
    work_image1.depth(16);
    work_image2.depth(16);

    //string image_ri_digest1 = image_ri_sha256(work_image1);
    //cout << "ri digest1: " << binary_to_hex(image_ri_digest1,CryptoPP::SHA256::DIGESTSIZE) << endl;
    //string image_ri_digest2 = image_ri_sha256(work_image2);
    //cout << "ri digest2: " << binary_to_hex(image_ri_digest2,CryptoPP::SHA256::DIGESTSIZE) << endl;

    string image_rfi_digest1 = image_rfi_sha256(work_image1);
    cout << "rfi digest1: " << binary_to_hex(image_rfi_digest1,CryptoPP::SHA256::DIGESTSIZE) << endl;
    string image_rfi_digest2 = image_rfi_sha256(work_image2);
    cout << "rfi digest2: " << binary_to_hex(image_rfi_digest2,CryptoPP::SHA256::DIGESTSIZE) << endl;

    return 0;

  }catch(Exception &error_){
    cout << "Caught exception: " << error_.what() << endl;
    return 1;
  }
  return 0;
}

