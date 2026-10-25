// Independent struct/hashlib hashes; regenerate with tools/reference_raw_streaming.py.
#pragma once
namespace raw_streaming_reference {
struct Case { long long width,height; const char* decoded; const char* input; };
inline constexpr Case cases[]={
    {400,200,"5b64037e7fa65ef70332ddd62c0a39fddc4ff869cd698219222fd58d4f9f1850","b8f8691120df94d53eb7470fc1bb954cce705bc59ee8d49c8e0d4c78dcd849e1"},
    {10000,3,"450b01b752d2f8c795dd65c053e91aacc21cc6f96c8c7e21c5c41572fa7907cc","0afa66967eea872969c65ca7d9a86cc756889ee27214b475df484d1e98df5269"},
};
}
