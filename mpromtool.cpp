#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdint.h>
#if defined(_WIN32)
#include <direct.h>
#else
#include <sys/stat.h>
#endif
#include <iostream>
#include <string>
#include <map>
#include <vector>
#include <algorithm>
#include "tinyxml2.h"

#define BIT_ALIGN(V, N) (V + ((N - (V % N)) % N))

struct FileData {
    uint16_t dir;
    uint16_t file;
    uint32_t comp_type;
    std::vector<uint8_t> data;
};
struct FileDataSegment {
    std::string segname;
    uint32_t romaddr = 0;
    std::map<uint16_t, std::string> datadir_map;
    std::vector<std::vector<FileData>> files;
};

struct MessDataDir {
    uint16_t id;
    std::vector<uint8_t> data;
};
struct MessDataSegment {
    std::string segname;
    bool use_dirmap = false;
    uint32_t romaddr = 0;
    bool new_format = false;
    std::vector<MessDataDir> mess_dir_all; //Only used if new_format == true
    std::vector<uint8_t> full_data; //Only used if new_format == false
};

struct HvqDataSegment {
    std::string segname;
    uint32_t romaddr = 0;
    std::map<uint32_t, std::string> hvqbg_map;
    std::vector<std::vector<uint8_t>> hvq_data;
};

struct BgAnimDataSegment {
    std::string segname;
    uint32_t romaddr = 0;
    std::map<uint32_t, std::string> bganim_map;
    std::vector<std::vector<uint8_t>> bganim_data;
};


struct SoundBankSegment
{
   std::string segname;
   uint32_t romaddr = 0;
   uint32_t size = 0;
   std::vector<uint8_t> data;
};

struct WaveTableSegment
{
   std::string segname;
   uint32_t romaddr = 0;
   uint32_t size = 0;
   std::vector<uint8_t> data;
};

struct SequenceSegment
{
   std::string segname;
   uint8_t unk0 = 0;    // TODO: new_format
   uint8_t unk1 = 0;    // TODO: new_format
   uint8_t bank = 0;
   int16_t id = -1;      // Some entries are copies
   uint32_t romaddr = 0;
   uint32_t size = 0;
   std::vector<uint8_t> data;
};

struct LibAudioSegment {
   std::string segname;
   uint32_t romaddr = 0;
   SoundBankSegment soundbankseg;
   WaveTableSegment wavetableseg;
   std::vector<SequenceSegment> seqsegs;
};

struct MusBankSegment {
    std::string segname;
    uint32_t romaddr = 0;
    bool new_format = false;
    std::vector<uint8_t> revision;
    std::vector<uint8_t> unkdata; // TODO: new_format
    LibAudioSegment libaudioseg;
};

struct SfxBankSegment {
    std::string segname;
    uint32_t romaddr = 0;
    bool new_format = false;
    std::vector<uint8_t> data;
};

struct FXDataSegment {
    std::string segname;
    uint32_t romaddr = 0;
    std::vector<uint8_t> data;
};

struct SegRef {
    std::string segname;
    unsigned int hi = 2;
    unsigned int lo = 6;
    uint32_t value = 0;
    bool end = false;
};

struct GameData {
    std::string game;
    FileDataSegment filedata;
    std::vector<MessDataSegment> messdata_all;
    std::map<uint32_t, std::string> messdata_dirmap;
    HvqDataSegment hvqdata;
    BgAnimDataSegment bganimdata;
    std::vector<MusBankSegment> musbanks;
    std::vector <SfxBankSegment> sfxbanks;
    FXDataSegment fxdata;
    std::map<std::string, uint32_t> segaddrs;
    std::vector<SegRef> segrefs;
};

std::string desc_path;
std::string game_id;
std::vector<uint8_t> rom_data;
GameData gamedata;

bool MakeDirectory(std::string dir)
{
    int ret;
#if defined(_WIN32)
    ret = _mkdir(dir.c_str());
#else 
    ret = mkdir(dir.c_str(), 0777); // notice that 777 is different than 0777
#endif]
    return ret != -1 || errno == EEXIST;
}


void PrintHelp(char* prog_name)
{
    std::cout << "Usage: " << prog_name << " [flags] args" << std::endl;
    std::cout << std::endl;
    std::cout << "-h/--help: Display this page" << std::endl;
    std::cout << "-d/--desc: Path to game description file directory (default gameconfig)" << std::endl;
    std::cout << "-b/--build: Build a new ROM" << std::endl;
    std::cout << "-a/--base: Path to base ROM" << std::endl;
}

void XMLCheck(tinyxml2::XMLError error)
{
    if (error != tinyxml2::XML_SUCCESS) {
        std::cout << "tinyxml2 error " << tinyxml2::XMLDocument::ErrorIDToName(error) << std::endl;
        exit(1);
    }
}

uint8_t ReadRom8(uint32_t offset)
{
    if (offset < rom_data.size()) {
        return rom_data[offset];
    } else {
        return 0;
    }
    
}

uint16_t ReadRom16(uint32_t offset)
{
    return (ReadRom8(offset) << 8) | ReadRom8(offset+1);
}

uint32_t ReadRom32(uint32_t offset)
{
    return (ReadRom16(offset) << 16) | ReadRom16(offset + 2);
}

std::string ReadRomGameID()
{
    std::string string;
    string.push_back(ReadRom8(59));
    string.push_back(ReadRom8(60));
    string.push_back(ReadRom8(61));
    string.push_back(ReadRom8(62));
    return string;
}

void ReadWholeFile(std::string path, std::vector<uint8_t>& data)
{
    FILE* file = fopen(path.c_str(), "rb");
    if (!file) {
        std::cout << "Failed to open " << path << " for reading." << std::endl;
        exit(1);
    }
    fseek(file, 0, SEEK_END);
    size_t size = ftell(file);
    if (size == 0) {
        fclose(file);
        data.clear();
        return;
    }
    data.resize(size);
    fseek(file, 0, SEEK_SET);
    fread(&data[0], 1, size, file);
    fclose(file);
}

void LoadROM(std::string path)
{
    ReadWholeFile(path, rom_data);
    if (ReadRom32(0) != 0x80371240) {
        std::cout << "File " << path << " is not a valid N64 ROM." << std::endl;
        exit(1);
    }
}

bool CompareSegRefs(SegRef& a, SegRef& b)
{
    return a.segname < b.segname;
}

void ParseSegRefsGameDesc(tinyxml2::XMLElement *segrefs)
{
    tinyxml2::XMLElement* segref_elem;
    if (!segrefs) {
        return;
    }
    segref_elem = segrefs->FirstChildElement("segref");
    while (segref_elem) {
        SegRef segref;
        uint16_t value_hi;
        int16_t value_lo;
        const char* segname_value;
        XMLCheck(segref_elem->QueryAttribute("segname", &segname_value));
        segref.segname = segname_value;
        XMLCheck(segref_elem->QueryAttribute("hi", &segref.hi));
        XMLCheck(segref_elem->QueryAttribute("lo", &segref.lo));
        segref_elem->QueryAttribute("end", &segref.end);
        value_hi = ReadRom16(segref.hi);
        value_lo = ReadRom16(segref.lo);
        segref.value = (value_hi << 16) + value_lo;
        gamedata.segrefs.push_back(segref);
        segref_elem = segref_elem->NextSiblingElement("segref");
    }
    std::sort(gamedata.segrefs.begin(), gamedata.segrefs.end(), CompareSegRefs);
}


bool CheckSegRefs()
{
    std::string curr_segname;
    uint32_t last_value = 0;
    bool is_end = false;
    for (size_t i = 0; i < gamedata.segrefs.size(); i++) {
        if (curr_segname != gamedata.segrefs[i].segname || is_end != gamedata.segrefs[i].end) {
            curr_segname = gamedata.segrefs[i].segname;
            last_value = gamedata.segrefs[i].value;
            is_end = gamedata.segrefs[i].end;
        } else {
            if (gamedata.segrefs[i].value != last_value) {
                return false;
            }
        }
    }
    return true;
}

uint32_t GetSegNameValue(std::string segname)
{
    for (size_t i = 0; i < gamedata.segrefs.size(); i++) {
        if (gamedata.segrefs[i].segname == segname && !gamedata.segrefs[i].end) {
            return gamedata.segrefs[i].value;
        }
    }
    std::cout << "Segment " << segname << " not in segrefs." << std::endl;
    return 0;
}

void SetSegNameValue(std::string segname, uint32_t value, bool end)
{
    for (size_t i = 0; i < gamedata.segrefs.size(); i++) {
        if (gamedata.segrefs[i].segname == segname && gamedata.segrefs[i].end == end) {
            gamedata.segrefs[i].value = value;
        }
    }
}

void ParseFileGameDesc(tinyxml2::XMLElement *element)
{
    if (!element) {
        std::cout << "Missing file data element." << std::endl;
        exit(1);
    }
    const char* segname_value;
    XMLCheck(element->QueryAttribute("segname", &segname_value));
    gamedata.filedata.segname = segname_value;
    gamedata.filedata.romaddr = GetSegNameValue(gamedata.filedata.segname);
    tinyxml2::XMLElement *child_elem = element->FirstChildElement("datadir");
    while (child_elem) {
        unsigned int id;
        const char* name;
        XMLCheck(child_elem->QueryAttribute("id", &id));
        XMLCheck(child_elem->QueryAttribute("name", &name));
        gamedata.filedata.datadir_map.insert({ id, name });
        child_elem = child_elem->NextSiblingElement("datadir");
    }
}

void ParseMessDataRomGameDesc(tinyxml2::XMLElement* element)
{
    if (!element) {
        return;
    }
    const char* segname_value;
    MessDataSegment messdata;
    XMLCheck(element->QueryAttribute("segname", &segname_value));
    element->QueryAttribute("use_dirmap", &messdata.use_dirmap);
    messdata.segname = segname_value;
    messdata.romaddr = GetSegNameValue(messdata.segname);
    messdata.new_format = game_id == "mp3";
    gamedata.messdata_all.push_back(messdata);
}

void ParseMessDataRomDirMap(tinyxml2::XMLElement* element)
{
    if (!element) {
        return;
    }
    if (game_id != "mp3") {
        std::cout << "Message Data Directory Map should only be present in Mario Party 3." << std::endl;
        exit(1);
    }
    tinyxml2::XMLElement* child_elem = element->FirstChildElement("messdir");
    while (child_elem) {
        unsigned int id;
        const char* name;
        XMLCheck(child_elem->QueryAttribute("id", &id));
        XMLCheck(child_elem->QueryAttribute("name", &name));
        gamedata.messdata_dirmap.insert({ id, name });
        child_elem = child_elem->NextSiblingElement("messdir");
    }
}

void ParseHvqDataRomGameDesc(tinyxml2::XMLElement* element)
{
    if (!element) {
        std::cout << "Missing HVQ data element." << std::endl;
        exit(1);
    }
    const char* segname_value;
    XMLCheck(element->QueryAttribute("segname", &segname_value));
    gamedata.hvqdata.segname = segname_value;
    gamedata.hvqdata.romaddr = GetSegNameValue(gamedata.hvqdata.segname);
    tinyxml2::XMLElement* child_elem = element->FirstChildElement("hvqbg");
    while (child_elem) {
        unsigned int id;
        const char* name;
        XMLCheck(child_elem->QueryAttribute("id", &id));
        XMLCheck(child_elem->QueryAttribute("name", &name));
        gamedata.hvqdata.hvqbg_map.insert({ id, name });
        child_elem = child_elem->NextSiblingElement("hvqbg");
    }
}

void ParseBgAnimDataRomGameDesc(tinyxml2::XMLElement* element)
{
    if (!element) {
        if (game_id == "mp2") {
            std::cout << "Missing Background Animation data element." << std::endl;
            exit(1);
        }
        return;
    }
    const char* segname_value;
    XMLCheck(element->QueryAttribute("segname", &segname_value));
    gamedata.bganimdata.segname = segname_value;
    gamedata.bganimdata.romaddr = GetSegNameValue(gamedata.bganimdata.segname);
    tinyxml2::XMLElement* child_elem = element->FirstChildElement("bganim");
    while (child_elem) {
        unsigned int id;
        const char* name;
        XMLCheck(child_elem->QueryAttribute("id", &id));
        XMLCheck(child_elem->QueryAttribute("name", &name));
        gamedata.bganimdata.bganim_map.insert({ id, name });
        child_elem = child_elem->NextSiblingElement("bganim");
    }
}

void ParseMusBankGameDesc(tinyxml2::XMLElement* element)
{
    if (!element) {
        return;
    }
    const char* segname_value;
    MusBankSegment musbank;
    XMLCheck(element->QueryAttribute("segname", &segname_value));
    musbank.segname = segname_value;
    musbank.romaddr = GetSegNameValue(musbank.segname);
    musbank.new_format = (game_id == "mp2" || game_id == "mp3");
    if (musbank.new_format) {
       musbank.revision.push_back(0x4D); // M
       musbank.revision.push_back(0x42); // B
       musbank.revision.push_back(0x46); // F
       musbank.revision.push_back(0x30); // 0
    }
    else {
       musbank.revision.push_back(0x53); // S
       musbank.revision.push_back(0x32); // 1/2
    }
    gamedata.musbanks.push_back(musbank);
}

void ParseSfxBankGameDesc(tinyxml2::XMLElement* element)
{
    if (!element) {
        return;
    }
    const char* segname_value;
    SfxBankSegment sfxbank;
    XMLCheck(element->QueryAttribute("segname", &segname_value));
    sfxbank.segname = segname_value;
    sfxbank.romaddr = GetSegNameValue(sfxbank.segname);
    sfxbank.new_format = (game_id == "mp2" || game_id == "mp3");
    gamedata.sfxbanks.push_back(sfxbank);
}

void ParseFXDataRomGameDesc(tinyxml2::XMLElement* element)
{
    if (!element) {
        std::cout << "Missing FX Data Element." << std::endl;
        exit(1);
    }
    const char* segname_value;
    XMLCheck(element->QueryAttribute("segname", &segname_value));
    gamedata.fxdata.segname = segname_value;
    gamedata.fxdata.romaddr = GetSegNameValue(gamedata.fxdata.segname);
}

void ParseGameDesc(tinyxml2::XMLElement* root)
{
    const char* game_name_value;
    XMLCheck(root->QueryAttribute("game", &game_name_value));
    game_id = game_name_value;
    ParseSegRefsGameDesc(root->FirstChildElement("segrefs"));
    if (!CheckSegRefs()) {
        std::cout << "Invalid segment references" << std::endl;
        exit(1);
    }
    ParseFileGameDesc(root->FirstChildElement("filedata"));
    tinyxml2::XMLElement* element = root->FirstChildElement("messdata");
    while (element) {
        ParseMessDataRomGameDesc(element);
        element = element->NextSiblingElement("messdata");
    }
    if (gamedata.messdata_all.size() == 0) {
        std::cout << "No Message Data Elements." << std::endl;
        exit(1);
    }
    ParseMessDataRomDirMap(root->FirstChildElement("messdir_map"));
    ParseHvqDataRomGameDesc(root->FirstChildElement("hvqdata"));
    ParseBgAnimDataRomGameDesc(root->FirstChildElement("bganimdata"));
    element = root->FirstChildElement("musbank");
    while (element) {
        ParseMusBankGameDesc(element);
        element = element->NextSiblingElement("musbank");
    }
    if (gamedata.musbanks.size() == 0) {
        std::cout << "No Music Bank Elements." << std::endl;
        exit(1);
    }
    element = root->FirstChildElement("sfxbank");
    while (element) {
        ParseSfxBankGameDesc(element);
        element = element->NextSiblingElement("sfxbank");
    }
    if (gamedata.sfxbanks.size() == 0) {
        std::cout << "No Sound Effect Bank Elements." << std::endl;
        exit(1);
    }
    ParseFXDataRomGameDesc(root->FirstChildElement("fxdata"));
}

void ReadGameDesc(std::string gameid)
{
    std::string desc_file = desc_path + "\\game_" + gameid + ".xml";
    tinyxml2::XMLDocument document;
    if (document.LoadFile(desc_file.c_str()) != tinyxml2::XML_SUCCESS) {
        std::cout << "Failed to load Game Description file for game ID " << gameid << std::endl;
        exit(1);
    }
    tinyxml2::XMLElement* root = document.FirstChildElement("gamedesc");
    if (!root) {
        std::cout << "Invalid Game Desription file." << std::endl;
        exit(1);
    }
    ParseGameDesc(root);
}

size_t DecodeNone(size_t offset, size_t raw_size, std::vector<uint8_t>& data)
{
    size_t offset_start = offset;
    uint8_t* dst = &data[0];
    for (size_t i = 0; i < raw_size; i++) {
        *dst++ = ReadRom8(offset++);
    }
    return offset - offset_start;
}

size_t DecodeLZ(size_t offset, size_t raw_size, std::vector<uint8_t>& data)
{
    size_t offset_start = offset;
    uint8_t window[1024];
    uint32_t window_ofs = 958;
    uint16_t flag = 0;
    uint8_t* dst = &data[0];
    memset(window, 0, 1024);
    while (raw_size > 0) {
        flag >>= 1;
        if (!(flag & 0x100)) {
            flag = 0xFF00 | ReadRom8(offset++);
        }
        if (flag & 0x1) {
            window[window_ofs++] = *dst++ = ReadRom8(offset++);
            window_ofs %= 1024;
            raw_size--;
        }
        else {
            uint32_t i;
            uint8_t byte1 = ReadRom8(offset++);
            uint8_t byte2 = ReadRom8(offset++);
            uint32_t ofs = ((byte2 & 0xC0) << 2) | byte1;
            uint32_t copy_len = (byte2 & 0x3F) + 3;
            for (i = 0; i < copy_len; i++) {
                window[window_ofs++] = *dst++ = window[(ofs + i) % 1024];
                window_ofs %= 1024;
            }
            raw_size -= i;
        }
    }
    return offset - offset_start;
}

size_t DecodeSlide(size_t offset, size_t raw_size, std::vector<uint8_t>& data)
{
    size_t offset_start = offset;
    offset += 4;
    uint32_t num_bits = 0;
    uint32_t mask = 0;
    uint8_t* dst = &data[0];
    uint8_t* base_ptr = dst;
    while (raw_size > 0) {
        if (num_bits == 0) {
            mask = ReadRom32(offset);
            offset += 4;
            num_bits = 32;
        }
        if (mask & 0x80000000) {
            *dst++ = ReadRom8(offset++);
            raw_size--;
        } else {
            uint32_t copy_ofs = ReadRom16(offset);
            uint32_t copy_len = (copy_ofs & 0xF000) >> 12;
            uint8_t* lookback_ptr;
            
            copy_ofs &= 0xFFF;
            lookback_ptr = dst - copy_ofs;
            offset += 2;
            if (copy_len == 0) {
                copy_len = ReadRom8(offset++) + 18;
            }else {
                copy_len += 2;
            }
            raw_size -= copy_len;
            while (copy_len) {
                if (lookback_ptr - 1 < base_ptr) {
                    *dst++ = 0;
                }
                else {
                    *dst++ = *(lookback_ptr - 1);
                }
                copy_len--;
                lookback_ptr++;
            }
        }
        mask <<= 1;
        num_bits--;
    }
    return offset - offset_start;
}

size_t DecodeRle(size_t offset, size_t raw_size, std::vector<uint8_t>& data)
{
    size_t offset_start = offset;
    uint8_t* dst = &data[0];
    while (raw_size > 0) {
        uint8_t len_value = ReadRom8(offset++);
        if (len_value < 128) {
            uint8_t value = ReadRom8(offset++);
            for (uint8_t i = 0; i < len_value; i++) {
                *dst++ = value;
            }
        } else {
            len_value -= 128;
            for (uint8_t i = 0; i < len_value; i++) {
                *dst++ = ReadRom8(offset++);
            }
        }
        raw_size -= len_value;
    }
    return offset - offset_start;
}

size_t DecodeData(size_t offset, std::vector<uint8_t> &data)
{
    size_t raw_size = ReadRom32(offset);
    size_t comptype = ReadRom32(offset + 4);
    size_t comp_size = 0;
    offset += 8;
    data.resize(raw_size);
    switch (comptype) {
        case 0:
            comp_size = DecodeNone(offset, raw_size, data);
            break;

        case 1:
            comp_size = DecodeLZ(offset, raw_size, data);
            break;

        case 2:
            comp_size = DecodeSlide(offset, raw_size, data);
            break;

        case 3:
        case 4:
            comp_size = DecodeSlide(offset, raw_size, data);
            break;

        case 5:
            comp_size = DecodeRle(offset, raw_size, data);
            break;

        default:
            std::cout << "Unsupported decode type " << comptype << "." << std::endl;
            exit(1);
            break;
    }
    if (comp_size % 2 != 0) {
        comp_size++;
    }
    return comp_size + 8;
}


void ParseFileDataRom()
{
    size_t romaddr_base = gamedata.filedata.romaddr;
    size_t dircnt = ReadRom32(romaddr_base);
    for (size_t i = 0; i < dircnt; i++) {
        size_t diraddr_base = romaddr_base+ReadRom32(romaddr_base + 4 + (i * 4));
        size_t filecnt = ReadRom32(diraddr_base);
        std::vector<FileData> files;
        for (size_t j = 0; j < filecnt; j++) {
            size_t file_ofs = diraddr_base+ReadRom32(diraddr_base + 4 + (j * 4));
            FileData filedata;
            filedata.dir = i;
            filedata.file = j;
            filedata.comp_type = ReadRom32(file_ofs + 4);
            DecodeData(file_ofs, filedata.data);
            files.push_back(filedata);
        }
        gamedata.filedata.files.push_back(files);
    }
}

void ParseMessDataRom(MessDataSegment& messdata)
{
    if (messdata.new_format) {
        size_t dircnt = ReadRom32(messdata.romaddr);
        for (size_t i = 0; i < dircnt; i++) {
            size_t dir_ofs = messdata.romaddr + ReadRom32(messdata.romaddr + (i * 4) + 4);
            MessDataDir dir;
            dir.id = i;
            DecodeData(dir_ofs, dir.data);
            messdata.mess_dir_all.push_back(dir);
        }
    } else {
        size_t messcnt = ReadRom32(messdata.romaddr);
        size_t last_mess_ofs = ReadRom32(messdata.romaddr + ((messcnt - 1) * 4) + 4);
        uint16_t last_mess_size = ReadRom16(messdata.romaddr + last_mess_ofs)+2;
        size_t total_size = last_mess_ofs + last_mess_size;
        if (total_size % 2 != 0) {
            total_size++;
        }
        messdata.full_data.resize(total_size);
        memcpy(&messdata.full_data[0], &rom_data[messdata.romaddr], total_size);
    }
}

void ParseHvqDataRom()
{
    size_t romaddr_base = gamedata.hvqdata.romaddr;
    size_t dircnt = ReadRom32(romaddr_base);
    for (size_t i = 0; i < dircnt - 1; i++) {
        size_t start_ofs = romaddr_base + ReadRom32(romaddr_base + 4 + (i * 4));
        size_t end_ofs = romaddr_base + ReadRom32(romaddr_base + 4 + ((i + 1) * 4));
        size_t size = end_ofs - start_ofs;
        std::vector<uint8_t> data;
        data.resize(size);
        memcpy(&data[0], &rom_data[start_ofs], size);
        gamedata.hvqdata.hvq_data.push_back(data);
    }
}

void ParseBgAnimDataRom()
{
    size_t romaddr_base = gamedata.bganimdata.romaddr;
    size_t dircnt = ReadRom32(romaddr_base);
    for (size_t i = 0; i < dircnt - 1; i++) {
        size_t start_ofs = romaddr_base + ReadRom32(romaddr_base + 4 + (i * 4));
        size_t end_ofs = romaddr_base + ReadRom32(romaddr_base + 4 + ((i + 1) * 4));
        size_t size = end_ofs - start_ofs;
        std::vector<uint8_t> data;
        data.resize(size);
        memcpy(&data[0], &rom_data[start_ofs], size);
        gamedata.bganimdata.bganim_data.push_back(data);
    }
}

void LibAudioDataRom(LibAudioSegment& libaudioseg) {
   // Get Sound Bank
   libaudioseg.soundbankseg.data.resize(libaudioseg.soundbankseg.size);
   memcpy(&libaudioseg.soundbankseg.data[0], &rom_data[libaudioseg.soundbankseg.romaddr], libaudioseg.soundbankseg.size);

   // Get Wave Table
   libaudioseg.wavetableseg.data.resize(libaudioseg.wavetableseg.size);
   memcpy(&libaudioseg.wavetableseg.data[0], &rom_data[libaudioseg.wavetableseg.romaddr], libaudioseg.wavetableseg.size);

   // Get Sequences
   for (auto& seqseg : libaudioseg.seqsegs) {
      seqseg.data.resize(seqseg.size);
      memcpy(&seqseg.data[0], &rom_data[seqseg.romaddr], seqseg.size);
   }


}

void ParseMusBankDataRom(MusBankSegment &musbank)
{
    size_t romaddr_base = musbank.romaddr;
    if (musbank.new_format) {
        musbank.revision.push_back(0x4D); // M
        musbank.revision.push_back(0x42); // B
        musbank.revision.push_back(0x46); // F
        musbank.revision.push_back(0x30); // 0
        uint32_t count = ReadRom32(romaddr_base + 4);
        uint32_t tbl_record_ofs = romaddr_base + 64 + (count * 16) + 8;
        uint32_t snd_record_ofs = romaddr_base + 64 + (count * 16);
        musbank.libaudioseg.seqsegs.resize(count);
        uint32_t header_size = (count * 16) + 80;

        for (uint32_t i = 0; i < count; i++) {
           uint32_t seq_offset = ReadRom32(romaddr_base + 72 + i * 16);
           musbank.libaudioseg.seqsegs[i].romaddr = romaddr_base + seq_offset;
           musbank.libaudioseg.seqsegs[i].size = ReadRom32(romaddr_base + 76 + i * 16);
           musbank.libaudioseg.seqsegs[i].unk0 = ReadRom8(romaddr_base + 64 + i * 16);
           musbank.libaudioseg.seqsegs[i].unk1 = ReadRom8(romaddr_base + 65 + i * 16);
           musbank.libaudioseg.seqsegs[i].bank = ReadRom8(romaddr_base + 66 + i * 16);
        }
        musbank.libaudioseg.soundbankseg.romaddr = romaddr_base + ReadRom32(snd_record_ofs);
        musbank.libaudioseg.soundbankseg.size = ReadRom32(snd_record_ofs + 4);
        musbank.libaudioseg.wavetableseg.romaddr = romaddr_base + ReadRom32(tbl_record_ofs);
        musbank.libaudioseg.wavetableseg.size = ReadRom32(tbl_record_ofs + 4);
        musbank.unkdata.resize(0x40 - 0x8);
        memcpy(&musbank.unkdata[0], &rom_data[romaddr_base + 0x8], 0x40 - 0x8); // TODO: Parse this global music data
    } else {
        musbank.revision.push_back(0x53);      // S
        musbank.revision.push_back(0x32);      // 1/2
        uint16_t count = ReadRom16(romaddr_base + 2);
        uint32_t musheader_size = count * 24 + 4;
        musheader_size = BIT_ALIGN(musheader_size, 16); // 'FF' padding

        // Get libaudio segments romaddr starts and sizes
        musbank.libaudioseg.soundbankseg.romaddr = romaddr_base + musheader_size;
        musbank.libaudioseg.wavetableseg.romaddr = romaddr_base + ReadRom32(romaddr_base + count * 8 + 16);
        musbank.libaudioseg.soundbankseg.size = musbank.libaudioseg.wavetableseg.romaddr - musbank.libaudioseg.soundbankseg.romaddr;
        musbank.libaudioseg.seqsegs.resize(count);
        for (uint32_t i = 0; i < count; i++) {
           musbank.libaudioseg.seqsegs[i].romaddr = romaddr_base + ReadRom32(romaddr_base + 8*i + 4);
           musbank.libaudioseg.seqsegs[i].size    = ReadRom32(romaddr_base + 8 * i + 8);
           musbank.libaudioseg.seqsegs[i].bank    = ReadRom8(romaddr_base + count * 8 + 4 + i * 16);
        }
        musbank.libaudioseg.wavetableseg.size = musbank.libaudioseg.seqsegs[0].romaddr - musbank.libaudioseg.wavetableseg.romaddr;
    }
    LibAudioDataRom(musbank.libaudioseg);
}

void ParseSfxBankDataRom(SfxBankSegment& sfxbank)
{
    size_t romaddr_base = sfxbank.romaddr;
    if (sfxbank.new_format) {
        uint32_t last_file_hdr = romaddr_base + 64 + 44;
        size_t last_file_ofs = ReadRom32(last_file_hdr);
        size_t last_file_size = ReadRom32(last_file_hdr + 4);
        size_t size = last_file_ofs + last_file_size;
        sfxbank.data.resize(size);
        memcpy(&sfxbank.data[0], &rom_data[romaddr_base], size);
    } else {
        uint16_t count = ReadRom16(romaddr_base + 2);
        uint32_t last_file_hdr = romaddr_base + 4 + (count * 8) + 32;
        size_t last_file_ofs = ReadRom32(last_file_hdr);
        size_t last_file_size = ReadRom32(last_file_hdr + 4);
        size_t size = last_file_ofs + last_file_size;
        sfxbank.data.resize(size);
        memcpy(&sfxbank.data[0], &rom_data[romaddr_base], size);
    }
}

void ParseFXDataRom()
{
    size_t romaddr_base = gamedata.fxdata.romaddr;
    uint32_t count = ReadRom32(romaddr_base + 4);
    size_t size = (count * 0x208) + 16;
    gamedata.fxdata.data.resize(size);
    memcpy(&gamedata.fxdata.data[0], &rom_data[romaddr_base], size);
}

void ParseGameDataRom()
{
    ParseFileDataRom();
    for (size_t i = 0; i < gamedata.messdata_all.size(); i++) {
        ParseMessDataRom(gamedata.messdata_all[i]);
    }
    ParseHvqDataRom();
    if (game_id == "mp2") {
        ParseBgAnimDataRom();
    }
    
    for (size_t i = 0; i < gamedata.musbanks.size(); i++) {
        ParseMusBankDataRom(gamedata.musbanks[i]);
    }
    for (size_t i = 0; i < gamedata.sfxbanks.size(); i++) {
        ParseSfxBankDataRom(gamedata.sfxbanks[i]);
    }
    ParseFXDataRom();
}

std::string GetDataDirName(uint16_t index)
{
    if (gamedata.filedata.datadir_map.count(index) != 0) {
        return gamedata.filedata.datadir_map[index];
    } else {
        return std::to_string(index);
    }
}

std::string GetAutoDataExtension(size_t dir, size_t file)
{
    uint8_t hmf_magic[] = "HBINMODE";
    uint8_t mot_magic[] = "MTNX";
    uint8_t skn_magic[] = "MTSK";
    uint8_t hvq_mps_magic[] = "HVQ-MPS 1.1";
    uint8_t hvq2_magic[] = "HVQ 2.0";
    uint8_t hvqnew_magic[] = { 0, 0, 0, 48 };
    uint8_t anm_magic1[] = { 0, 0, 0, 32 };
    uint8_t anm_magic2[] = { 0, 0, 0, 27 };

    FileData &filedata = gamedata.filedata.files[dir][file];
    if (memcmp(&filedata.data[8], hmf_magic, 8) == 0) {
        return ".hmf";
    } else if (memcmp(&filedata.data[0], mot_magic, 4) == 0) {
        return ".mot";
    } else if (memcmp(&filedata.data[0], skn_magic, 4) == 0) {
        return ".skn";
    } else if (memcmp(&filedata.data[0], anm_magic1, 4) == 0 || memcmp(&filedata.data[0], anm_magic2, 4) == 0) {
        return ".anm";
    } else if (memcmp(&filedata.data[0], hvq2_magic, 4) == 0 || memcmp(&filedata.data[4], hvqnew_magic, 4) == 0) {
        return ".hvq";
    } else if (memcmp(&filedata.data[0], hvq_mps_magic, 4) == 0) {
        return ".hvqmps";
    } else {
        return ".bin";
    }
}
void DumpFileData(tinyxml2::XMLDocument& document, tinyxml2::XMLElement* root, std::string outdir)
{
    MakeDirectory(outdir);
    tinyxml2::XMLElement* filedata = document.NewElement("filedata");
    for (size_t i = 0; i < gamedata.filedata.files.size(); i++) {
        tinyxml2::XMLElement* datadir = document.NewElement("datadir");
        std::string dir_name = GetDataDirName(i);
        std::string dir = outdir + "/" + dir_name;
        MakeDirectory(dir);
        for (size_t j = 0; j < gamedata.filedata.files[i].size(); j++) {
            FileData& file = gamedata.filedata.files[i][j];
            std::string filepath = dir + "/" + std::to_string(j) + GetAutoDataExtension(i, j);
            tinyxml2::XMLElement* file_element = document.NewElement("file");
            FILE* out_file = fopen(filepath.c_str(), "wb");
            if (!out_file) {
                std::cout << "Failed to open " << filepath << " for writing." << std::endl;
                exit(1);
            }
            fwrite(&file.data[0], 1, file.data.size(), out_file);
            fclose(out_file);
            file_element->SetAttribute("path", filepath.c_str());
            file_element->SetAttribute("comptype", file.comp_type);
            datadir->InsertEndChild(file_element);
        }
        filedata->InsertEndChild(datadir);
    }
    root->InsertEndChild(filedata);
}

std::string GetMessDirName(size_t index)
{
    if (gamedata.messdata_dirmap.count(index) != 0) {
        return gamedata.messdata_dirmap[index];
    }
    else {
        return std::to_string(index);
    }
}

void DumpMessDataExt(tinyxml2::XMLDocument& document, tinyxml2::XMLElement* root, std::string basedir, size_t index)
{
    MessDataSegment& messdata = gamedata.messdata_all[index];
    std::string outdir = basedir + "/" + messdata.segname + "/";
    MakeDirectory(outdir);
    tinyxml2::XMLElement* messdata_element = document.NewElement("messdata");
    messdata_element->SetAttribute("new_format", true);
    messdata_element->SetAttribute("segindex", index);
    for (size_t i = 0; i < messdata.mess_dir_all.size(); i++) {
        tinyxml2::XMLElement* messdir = document.NewElement("messdir");
        std::string messdir_name = std::to_string(i);
        if (messdata.use_dirmap) {
            messdir_name = GetMessDirName(i);
        }
        std::string messfile = outdir + messdir_name + ".bin";
        FILE* out_file = fopen(messfile.c_str(), "wb");
        if (!out_file) {
            std::cout << "Failed to open " << messfile << " for writing." << std::endl;
            exit(1);
        }
        fwrite(&messdata.mess_dir_all[i].data[0], 1, messdata.mess_dir_all[i].data.size(), out_file);
        fclose(out_file);
        messdir->SetAttribute("path", messfile.c_str());
        messdata_element->InsertEndChild(messdir);
    }
    root->InsertEndChild(messdata_element);
}

void DumpMessData(tinyxml2::XMLDocument& document, tinyxml2::XMLElement* root, std::string basedir, size_t index)
{
    MessDataSegment& messdata = gamedata.messdata_all[index];
    std::string outfile = basedir + "/" + messdata.segname + ".bin";
    tinyxml2::XMLElement* messdata_element = document.NewElement("messdata");
    messdata_element->SetAttribute("new_format", false);
    messdata_element->SetAttribute("segindex", index);
    messdata_element->SetAttribute("path", outfile.c_str());
    FILE* out_file = fopen(outfile.c_str(), "wb");
    if (!out_file) {
        std::cout << "Failed to open " << outfile << " for writing." << std::endl;
        exit(1);
    }
    fwrite(&messdata.full_data[0], 1, messdata.full_data.size(), out_file);
    fclose(out_file);

    root->InsertEndChild(messdata_element);
}

std::string GetHvqBgName(size_t index)
{
    if (gamedata.hvqdata.hvqbg_map.count(index) != 0) {
        return gamedata.hvqdata.hvqbg_map[index];
    } else {
        return std::to_string(index);
    }
}

void DumpHvqData(tinyxml2::XMLDocument& document, tinyxml2::XMLElement* root, std::string outdir)
{
    MakeDirectory(outdir);
    tinyxml2::XMLElement* hvqdata = document.NewElement("hvqdata");
    for (size_t i = 0; i < gamedata.hvqdata.hvq_data.size(); i++) {
        tinyxml2::XMLElement* hvqbg = document.NewElement("hvqbg");
        std::string hvqfile = outdir + "/" + GetHvqBgName(i) + ".bghvq";
        FILE* out_file = fopen(hvqfile.c_str(), "wb");
        if (!out_file) {
            std::cout << "Failed to open " << hvqfile << " for writing." << std::endl;
            exit(1);
        }
        fwrite(&gamedata.hvqdata.hvq_data[i][0], 1, gamedata.hvqdata.hvq_data[i].size(), out_file);
        fclose(out_file);
        hvqbg->SetAttribute("path", hvqfile.c_str());
        hvqdata->InsertEndChild(hvqbg);
    }
    root->InsertEndChild(hvqdata);
}

std::string GetBgAnimName(size_t index)
{
    if (gamedata.bganimdata.bganim_map.count(index) != 0) {
        return gamedata.bganimdata.bganim_map[index];
    }
    else {
        return std::to_string(index);
    }
}

void DumpBgAnimData(tinyxml2::XMLDocument& document, tinyxml2::XMLElement* root, std::string outdir)
{
    MakeDirectory(outdir);
    tinyxml2::XMLElement* bganimdata = document.NewElement("bganimdata");
    for (size_t i = 0; i < gamedata.bganimdata.bganim_data.size(); i++) {
        std::vector<uint8_t>& segment = gamedata.bganimdata.bganim_data[i];
        tinyxml2::XMLElement* bganim = document.NewElement("bganim");
        std::string hvqfile = outdir + "/" + GetBgAnimName(i) + ".bganm";
        FILE* out_file = fopen(hvqfile.c_str(), "wb");
        if (!out_file) {
            std::cout << "Failed to open " << hvqfile << " for writing." << std::endl;
            exit(1);
        }
        fwrite(&segment[0], 1, segment.size(), out_file);
        fclose(out_file);
        bganim->SetAttribute("path", hvqfile.c_str());
        bganimdata->InsertEndChild(bganim);
    }
    root->InsertEndChild(bganimdata);
}

void DumpMusBank(tinyxml2::XMLDocument& document, tinyxml2::XMLElement* root, std::string basedir, size_t index)
{

    /* Save XML / Bins */
    MusBankSegment& musbank = gamedata.musbanks[index];
    std::string dir = basedir + "/" + musbank.segname;
    std::string seqbasedir = dir + "/seqs";
    MakeDirectory(basedir);
    MakeDirectory(dir);
    MakeDirectory(seqbasedir);
    tinyxml2::XMLElement* element = document.NewElement("musbank");
    std::string musbankfile = dir + "/musbank.xml";
    std::string soundbankfile = dir + "/soundbank.ctl";
    std::string wavetablefile = dir + "/wavetable.tbl";
    element->SetAttribute("segindex", index);
    element->SetAttribute("new_format", musbank.new_format);

    tinyxml2::XMLElement* soundbankele = element->InsertNewChildElement("soundbank");
    soundbankele->SetAttribute("path", soundbankfile.c_str());
    element->InsertEndChild(soundbankele);
    FILE* out_file = fopen(soundbankfile.c_str(), "wb");
    if (!out_file) {
       std::cout << "Failed to open " << soundbankfile << " for writing." << std::endl;
       exit(1);
    }
    fwrite(&musbank.libaudioseg.soundbankseg.data[0], 1, musbank.libaudioseg.soundbankseg.data.size(), out_file);
    fclose(out_file);

    tinyxml2::XMLElement* wavetableele = element->InsertNewChildElement("wavetable");
    wavetableele->SetAttribute("path", wavetablefile.c_str());
    element->InsertEndChild(wavetableele);
    out_file = fopen(wavetablefile.c_str(), "wb");
    if (!out_file) {
       std::cout << "Failed to open " << wavetablefile << " for writing." << std::endl;
       exit(1);
    }
    fwrite(&musbank.libaudioseg.wavetableseg.data[0], 1, musbank.libaudioseg.wavetableseg.data.size(), out_file);
    fclose(out_file);

    tinyxml2::XMLElement* seqbankele = element->InsertNewChildElement("seqbank");
    std::map<uint32_t, uint32_t> seqmap;
    uint32_t i = 0;
    for (auto& seq : musbank.libaudioseg.seqsegs) {
       uint32_t id = i;
       bool write = false;
       if (seqmap.find(seq.romaddr) == seqmap.end()) {
          seqmap[seq.romaddr] = i++;
          write = true;
       }
       else {
          id = seqmap[seq.romaddr];
       }

       std::string seqfile = seqbasedir + "/" + std::to_string(id) + ".seq";
       tinyxml2::XMLElement* seqelement = seqbankele->InsertNewChildElement("seq");
       seqelement->SetAttribute("path", seqfile.c_str());
       seqelement->SetAttribute("bank", seq.bank);
       if (musbank.new_format) {
          seqelement->SetAttribute("unk0", seq.unk0);
          seqelement->SetAttribute("unk1", seq.unk1);
       }
       seqbankele->InsertEndChild(seqelement);

       if (write) {
          out_file = fopen(seqfile.c_str(), "wb");
          if (!out_file) {
             std::cout << "Failed to open " << seqfile << " for writing." << std::endl;
             exit(1);
          }
          fwrite(&seq.data[0], 1, seq.data.size(), out_file);
          fclose(out_file);
       }
    }
    element->InsertEndChild(seqbankele);

    // TODO: Remove and parse this information for new_format
    if (musbank.new_format) {
       std::string unkfile = dir + "/unkdata.bin";
       element->SetAttribute("unkdata_path", unkfile.c_str());
       out_file = fopen(unkfile.c_str(), "wb");
       if (!out_file) {
          std::cout << "Failed to open " << unkfile << " for writing." << std::endl;
          exit(1);
       }
       fwrite(&musbank.unkdata[0], 1, musbank.unkdata.size(), out_file);
       fclose(out_file);
    }

    root->InsertEndChild(element);
}

void DumpSfxBank(tinyxml2::XMLDocument& document, tinyxml2::XMLElement* root, std::string basedir, size_t index)
{
    SfxBankSegment& sfxbank = gamedata.sfxbanks[index];
    std::string outfile = basedir + "/" + sfxbank.segname + ".bin";
    tinyxml2::XMLElement* element = document.NewElement("sfxbank");
    element->SetAttribute("path", outfile.c_str());
    element->SetAttribute("segindex", index);
    element->SetAttribute("new_format", sfxbank.new_format);
    FILE* out_file = fopen(outfile.c_str(), "wb");
    if (!out_file) {
        std::cout << "Failed to open " << outfile << " for writing." << std::endl;
        exit(1);
    }
    fwrite(&sfxbank.data[0], 1, sfxbank.data.size(), out_file);
    fclose(out_file);

    root->InsertEndChild(element);
}

void DumpFXData(tinyxml2::XMLDocument& document, tinyxml2::XMLElement* root, std::string basedir)
{
    std::string outfile = basedir + "/" + gamedata.fxdata.segname + ".bin";
    tinyxml2::XMLElement* element = document.NewElement("fxdata");
    element->SetAttribute("path", outfile.c_str());
    FILE* out_file = fopen(outfile.c_str(), "wb");
    if (!out_file) {
        std::cout << "Failed to open " << outfile << " for writing." << std::endl;
        exit(1);
    }
    fwrite(&gamedata.fxdata.data[0], 1, gamedata.fxdata.data.size(), out_file);
    fclose(out_file);

    root->InsertEndChild(element);
}

void DumpGameData(std::string output)
{
    //Create listing
    tinyxml2::XMLDocument document;
    tinyxml2::XMLElement* root = document.NewElement("romdata");
    document.InsertFirstChild(root);
    MakeDirectory(output);
    DumpFileData(document, root, output + "/filedata");
    for (size_t i = 0; i < gamedata.messdata_all.size(); i++) {
        if (gamedata.messdata_all[i].new_format) {
            DumpMessDataExt(document, root, output, i);
        }
        else {
            DumpMessData(document, root, output, i);
        }
    }
    DumpHvqData(document, root, output + "/hvqdata");
    if (game_id == "mp2") {
        DumpBgAnimData(document, root, output + "/bganimdata");
    }
    for (size_t i = 0; i < gamedata.musbanks.size(); i++) {
        DumpMusBank(document, root, output + "/musdata", i);
    }
    for (size_t i = 0; i < gamedata.sfxbanks.size(); i++) {
        DumpSfxBank(document, root, output, i);
    }
    DumpFXData(document, root, output);
    //Try to save listing file
    std::string out_xml = output + "/romdata.xml";
    XMLCheck(document.SaveFile(out_xml.c_str()));

}
void ExtractROM(std::string output)
{
    ParseGameDataRom();
    DumpGameData(output);
}

void ParseFileData(tinyxml2::XMLElement *element)
{
    if (!element) {
        std::cout << "Missing file data element." << std::endl;
        exit(1);
    }
    tinyxml2::XMLElement* child_elem = element->FirstChildElement("datadir");
    while (child_elem) {
        std::vector<FileData> files;
        tinyxml2::XMLElement* file_elem = child_elem->FirstChildElement("file");
        while (file_elem) {
            FileData file;
            unsigned int comp_type;
            const char* path;
            file.dir = gamedata.filedata.files.size();
            file.file = files.size();
            XMLCheck(file_elem->QueryAttribute("comptype", &comp_type));
            XMLCheck(file_elem->QueryAttribute("path", &path));
            file.comp_type = comp_type;
            ReadWholeFile(path, file.data);
            files.push_back(file);
            file_elem = file_elem->NextSiblingElement("file");
        }
        gamedata.filedata.files.push_back(files);
        child_elem = child_elem->NextSiblingElement("datadir");
    }
}

void ParseMessData(tinyxml2::XMLElement* element)
{
    bool new_format;
    int seg_index;
    XMLCheck(element->QueryAttribute("segindex", &seg_index));
    XMLCheck(element->QueryAttribute("new_format", &new_format));
    MessDataSegment& seg = gamedata.messdata_all[seg_index];
    seg.new_format = new_format;
    if (new_format) {
        tinyxml2::XMLElement* dir_elem = element->FirstChildElement("messdir");
        while (dir_elem) {
            MessDataDir dir;
            const char* path;
            dir.id = seg.mess_dir_all.size();
            XMLCheck(dir_elem->QueryAttribute("path", &path));
            ReadWholeFile(path, dir.data);
            seg.mess_dir_all.push_back(dir);
            dir_elem = dir_elem->NextSiblingElement("messdir");
        }
    }
    else {
        const char* path;
        XMLCheck(element->QueryAttribute("path", &path));
        ReadWholeFile(path, seg.full_data);
    }
}

void ParseHvqData(tinyxml2::XMLElement* element)
{
    if (!element) {
        std::cout << "Missing HVQ data element." << std::endl;
        exit(1);
    }
    tinyxml2::XMLElement* child_elem = element->FirstChildElement("hvqbg");
    while (child_elem) {
        const char* path;
        std::vector<uint8_t> data;
        XMLCheck(child_elem->QueryAttribute("path", &path));
        ReadWholeFile(path, data);
        gamedata.hvqdata.hvq_data.push_back(data);
        child_elem = child_elem->NextSiblingElement("hvqbg");
    }
}

void ParseBgAnimData(tinyxml2::XMLElement* element)
{
    if (!element) {
        if (game_id == "mp2") {
            std::cout << "Missing Background Animation data element." << std::endl;
            exit(1);
        }
        return;
    }
    tinyxml2::XMLElement* child_elem = element->FirstChildElement("bganim");
    while (child_elem) {
        const char* path;
        std::vector<uint8_t> data;
        XMLCheck(child_elem->QueryAttribute("path", &path));
        ReadWholeFile(path, data);
        gamedata.bganimdata.bganim_data.push_back(data);
        child_elem = child_elem->NextSiblingElement("bganim");
    }
}

void ParseMusBank(tinyxml2::XMLElement* element)
{
    bool new_format;
    int seg_index;
    XMLCheck(element->QueryAttribute("segindex", &seg_index));
    XMLCheck(element->QueryAttribute("new_format", &new_format));
    tinyxml2::XMLElement* soundbankele = element->FirstChildElement("soundbank");
    tinyxml2::XMLElement* wavetableele = element->FirstChildElement("wavetable");
    tinyxml2::XMLElement* seqbankele = element->FirstChildElement("seqbank");
    MusBankSegment& seg = gamedata.musbanks[seg_index];
    seg.new_format = new_format;

    // TODO change parsing
    const char* soundbankpath;
    XMLCheck(soundbankele->QueryAttribute("path", &soundbankpath));
    ReadWholeFile(soundbankpath, seg.libaudioseg.soundbankseg.data);

    const char* wavetablepath;
    XMLCheck(wavetableele->QueryAttribute("path", &wavetablepath));
    ReadWholeFile(wavetablepath, seg.libaudioseg.wavetableseg.data);

    uint32_t count = seqbankele->ChildElementCount();
    seg.libaudioseg.seqsegs.resize(count);

    uint32_t i = 0;
    tinyxml2::XMLElement* seqele = seqbankele->FirstChildElement("seq");
    std::map<std::string, uint32_t> seqmap;
    while (seqele) {
       const char* seqpath;
       int bank;
       SequenceSegment& seqseg = seg.libaudioseg.seqsegs[i];
       XMLCheck(seqele->QueryAttribute("path", &seqpath));
       XMLCheck(seqele->QueryIntAttribute("bank", &bank));
       if (seg.new_format) {
          int unk0, unk1;
          XMLCheck(seqele->QueryIntAttribute("unk0", &unk0));
          XMLCheck(seqele->QueryIntAttribute("unk1", &unk1));
          seqseg.unk0 = unk0;
          seqseg.unk1 = unk1;
       }
       seqseg.bank = bank;
       if (seqmap.find(seqpath) == seqmap.end()) {
          ReadWholeFile(seqpath, seqseg.data);
          seqmap[seqpath] = i; // current index
       }
       else {
          seqseg.id = seqmap[seqpath]; // id with copy data
       }
       seqele = seqele->NextSiblingElement("seq");
       i++;
    }

    if (seg.new_format) {
       const char* unkdatapath;
       XMLCheck(element->QueryAttribute("unkdata_path", &unkdatapath));
       ReadWholeFile(unkdatapath, seg.unkdata);
    }
}

void ParseSfxBank(tinyxml2::XMLElement* element)
{
    bool new_format;
    int seg_index;
    XMLCheck(element->QueryAttribute("segindex", &seg_index));
    XMLCheck(element->QueryAttribute("new_format", &new_format));
    SfxBankSegment& seg = gamedata.sfxbanks[seg_index];
    seg.new_format = new_format;
    const char* path;
    XMLCheck(element->QueryAttribute("path", &path));
    ReadWholeFile(path, seg.data);
}

void ParseFxData(tinyxml2::XMLElement* element)
{
    if (!element) {
        std::cout << "Missing FX Data element." << std::endl;
        exit(1);
    }
    const char* path;
    XMLCheck(element->QueryAttribute("path", &path));
    ReadWholeFile(path, gamedata.fxdata.data);
}

void ParseRomData(std::string src_file)
{
    tinyxml2::XMLDocument document;
    XMLCheck(document.LoadFile(src_file.c_str()));
    tinyxml2::XMLElement* root = document.FirstChildElement("romdata");
    if (!root) {
        std::cout << "Invalid ROM Data file." << std::endl;
        exit(1);
    }
    ParseFileData(root->FirstChildElement("filedata"));
    tinyxml2::XMLElement* element = root->FirstChildElement("messdata");
    while (element) {
        ParseMessData(element);
        element = element->NextSiblingElement("messdata");
    }
    ParseHvqData(root->FirstChildElement("hvqdata"));
    ParseBgAnimData(root->FirstChildElement("bganimdata"));
    element = root->FirstChildElement("musbank");
    while (element) {
        ParseMusBank(element);
        element = element->NextSiblingElement("musbank");
    }
    element = root->FirstChildElement("sfxbank");
    while (element) {
        ParseSfxBank(element);
        element = element->NextSiblingElement("sfxbank");
    }
    ParseFxData(root->FirstChildElement("fxdata"));
}


void WriteU8(FILE* file, uint8_t value)
{
    fwrite(&value, 1, 1, file);
}

void WriteU16(FILE* file, uint16_t value)
{
    uint8_t temp[2];
    temp[0] = value >> 8;
    temp[1] = value & 0xFF;
    fwrite(temp, 2, 1, file);
}

void WriteU16At(FILE* file, uint16_t value, size_t offset)
{
    size_t prev_ofs = ftell(file);
    uint8_t temp[2];
    temp[0] = value >> 8;
    temp[1] = value & 0xFF;
    fseek(file, offset, SEEK_SET);
    fwrite(temp, 2, 1, file);
    fseek(file, prev_ofs, SEEK_SET);
}

void WriteU32(FILE* file, uint32_t value)
{
    uint8_t temp[4];
    temp[0] = value >> 24;
    temp[1] = (value >> 16) & 0xFF;
    temp[2] = (value >> 8) & 0xFF;
    temp[3] = value & 0xFF;
    fwrite(temp, 4, 1, file);
}

void WriteU32At(FILE* file, uint32_t value, size_t offset)
{
    size_t prev_ofs = ftell(file);
    uint8_t temp[4];
    temp[0] = value >> 24;
    temp[1] = (value >> 16) & 0xFF;
    temp[2] = (value >> 8) & 0xFF;
    temp[3] = value & 0xFF;
    fseek(file, offset, SEEK_SET);
    fwrite(temp, 4, 1, file);
    fseek(file, prev_ofs, SEEK_SET);
}

void WriteRawBuffer(FILE* file, std::vector<uint8_t>& data)
{
    fwrite(&data[0], 1, data.size(), file);
}

void WriteAlign(FILE* file, size_t align)
{
    while ((ftell(file) % align) != 0) {
        WriteU8(file, 0);
    }
}

void WriteAlignFF(FILE* file, size_t align)
{
   while ((ftell(file) % align) != 0) {
      WriteU8(file, 0xFF);
   }
}

#define N 1024   /* size of ring buffer */   
#define F 66   /* upper limit for match_length */   
#define THRESHOLD 2 /* encode string into position and length  if match_length is greater than this */
#define NIL  N /* index for root of binary search trees */   

uint8_t text_buf[N + F - 1];    /* ring buffer of size N,
        with extra F-1 bytes to facilitate string comparison */
int match_position, match_length,  /* of longest match.  These are
                        set by the InsertNode() procedure. */
    lson[N + 1], rson[N + 257], dad[N + 1];  /* left & right children &
            parents -- These constitute binary search trees. */

void InitTree(void)  /* initialize trees */
{
    int  i;

    /* For i = 0 to N - 1, rson[i] and lson[i] will be the right and
       left children of node i.  These nodes need not be initialized.
       Also, dad[i] is the parent of node i.  These are initialized to
       NIL (= N), which stands for 'not used.'
       For i = 0 to 255, rson[N + i + 1] is the root of the tree
       for strings that begin with character i.  These are initialized
       to NIL.  Note there are 256 trees. */

    for (i = N + 1; i <= N + 256; i++) rson[i] = NIL;
    for (i = 0; i < N; i++) dad[i] = NIL;
}

void InsertNode(int r)
/* Inserts string of length F, text_buf[r..r+F-1], into one of the
   trees (text_buf[r]'th tree) and returns the longest-match position
   and length via the global variables match_position and match_length.
   If match_length = F, then removes the old node in favor of the new
   one, because the old one will be deleted sooner.
   Note r plays double role, as tree node and position in buffer. */
{
    int  i, p, cmp;
    uint8_t* key;

    cmp = 1;  key = &text_buf[r];  p = N + 1 + key[0];
    rson[r] = lson[r] = NIL;  match_length = 0;
    for (; ; ) {
        if (cmp >= 0) {
            if (rson[p] != NIL) p = rson[p];
            else { rson[p] = r;  dad[r] = p;  return; }
        }
        else {
            if (lson[p] != NIL) p = lson[p];
            else { lson[p] = r;  dad[r] = p;  return; }
        }
        for (i = 1; i < F; i++)
            if ((cmp = key[i] - text_buf[p + i]) != 0)  break;
        if (i > match_length) {
            match_position = p;
            if ((match_length = i) >= F)  break;
        }
    }
    dad[r] = dad[p];  lson[r] = lson[p];  rson[r] = rson[p];
    dad[lson[p]] = r;  dad[rson[p]] = r;
    if (rson[dad[p]] == p) rson[dad[p]] = r;
    else                   lson[dad[p]] = r;
    dad[p] = NIL;  /* remove p */
}

void DeleteNode(int p)  /* deletes node p from tree */
{
    int  q;

    if (dad[p] == NIL) return;  /* not in tree */
    if (rson[p] == NIL) q = lson[p];
    else if (lson[p] == NIL) q = rson[p];
    else {
        q = lson[p];
        if (rson[q] != NIL) {
            do { q = rson[q]; } while (rson[q] != NIL);
            rson[dad[q]] = lson[q];  dad[lson[q]] = dad[q];
            lson[q] = lson[p];  dad[lson[p]] = q;
        }
        rson[q] = rson[p];  dad[rson[p]] = q;
    }
    dad[q] = dad[p];
    if (rson[dad[p]] == p) rson[dad[p]] = q;  else lson[dad[p]] = q;
    dad[p] = NIL;
}

void EncodeLZSS(FILE* dst_file, std::vector<uint8_t> &src)
{
    int  i, c, len, r, s, last_match_length, code_buf_ptr;
    uint8_t code_buf[17], mask;
    size_t src_pos = 0;

    InitTree();  /* initialize trees */
    code_buf[0] = 0;  /* code_buf[1..16] saves eight units of code, and
            code_buf[0] works as eight flags, "1" representing that the unit
            is an unencoded letter (1 byte), "0" a position-and-length pair
            (2 bytes).  Thus, eight units require at most 16 bytes of code. */
    code_buf_ptr = mask = 1;
    s = 0;  r = N - F;
    for (i = s; i < r; i++) text_buf[i] = '\0';  /* Clear the buffer with
            any character that will appear often. */
    for (len = 0; len < F && src_pos < src.size(); len++)
        text_buf[r + len] = c = src[src_pos++];  /* Read F bytes into the last F bytes of
                the buffer */
    if (len == 0) return;  /* text of size zero */
    for (i = 1; i <= F; i++) InsertNode(r - i);  /* Insert the F strings,
            each of which begins with one or more 'space' characters.  Note
            the order in which these strings are inserted.  This way,
            degenerate trees will be less likely to occur. */
    InsertNode(r);  /* Finally, insert the whole string just read.  The
            global variables match_length and match_position are set. */
    do {
        if (match_length > len) match_length = len;  /* match_length
                may be spuriously long near the end of text. */
        if (match_length <= THRESHOLD) {
            match_length = 1;  /* Not long enough match.  Send one byte. */
            code_buf[0] |= mask;  /* 'send one byte' flag */
            code_buf[code_buf_ptr++] = text_buf[r];  /* Send uncoded. */
        }
        else {
            code_buf[code_buf_ptr++] = (uint8_t)match_position;
            code_buf[code_buf_ptr++] = (uint8_t)
                (((match_position >> 2) & 0xC0)
                    | (match_length - (THRESHOLD + 1)));  /* Send position and
                                  length pair. Note match_length > THRESHOLD. */
        }
        if ((mask <<= 1) == 0) {  /* Shift mask left one bit. */
            for (i = 0; i < code_buf_ptr; i++)  /* Send at most 8 units of */
                putc(code_buf[i], dst_file);     /* code together */
            code_buf[0] = 0;  code_buf_ptr = mask = 1;
        }
        last_match_length = match_length;
        for (i = 0; i < last_match_length &&
            src_pos < src.size(); i++) {
            DeleteNode(s);          /* Delete old strings and */
            text_buf[s] = c = src[src_pos++];        /* read new bytes */
            if (s < F - 1) text_buf[s + N] = c;  /* If the position is
                    near the end of buffer, extend the buffer to make
                    string comparison easier. */
            s = (s + 1) & (N - 1);  r = (r + 1) & (N - 1);
            /* Since this is a ring buffer, increment the position
               modulo N. */
            InsertNode(r);  /* Register the string in text_buf[r..r+F-1] */
        }
        while (i++ < last_match_length) {       /* After the end of text, */
            DeleteNode(s);                                  /* no need to read, but */
            s = (s + 1) & (N - 1);  r = (r + 1) & (N - 1);
            if (--len) InsertNode(r);               /* buffer may not be empty. */
        }
    } while (len > 0);      /* until length of string to be processed is zero */
    if (code_buf_ptr > 1) {         /* Send remaining code. */
        for (i = 0; i < code_buf_ptr; i++) putc(code_buf[i], dst_file);
    }
}

void EncodeNone(FILE* file, std::vector<uint8_t>& data)
{
    WriteRawBuffer(file, data);
}


// simple and straight encoding scheme for Yaz0
uint32_t simpleEnc(uint8_t* src, uint32_t size, uint32_t pos, uint32_t* pMatchPos)
{
    uint32_t startPos = pos - 0x1000;
    uint32_t numBytes = 1;
    uint32_t matchPos = 0;
    uint32_t i;
    uint32_t j;

    if (pos < 0x1000)
        startPos = 0;
    for (i = startPos; i < pos; i++)
    {
        for (j = 0; j < size - pos; j++)
        {
            if (src[i + j] != src[j + pos])
                break;
        }
        if (j > numBytes)
        {
            numBytes = j;
            matchPos = i;
        }
    }
    *pMatchPos = matchPos;
    if (numBytes == 2)
        numBytes = 1;
    return numBytes;
}

// a lookahead encoding scheme for ngc Yaz0
uint32_t nintendoEnc(uint8_t* src, uint32_t size, uint32_t pos, uint32_t* pMatchPos)
{
    uint32_t numBytes = 1;
    static uint32_t numBytes1;
    static uint32_t matchPos;
    static int prevFlag = 0;

    // if prevFlag is set, it means that the previous position was determined by look-ahead try.
    // so just use it. this is not the best optimization, but nintendo's choice for speed.
    if (prevFlag == 1) {
        *pMatchPos = matchPos;
        prevFlag = 0;
        return numBytes1;
    }
    prevFlag = 0;
    numBytes = simpleEnc(src, size, pos, &matchPos);
    *pMatchPos = matchPos;

    // if this position is RLE encoded, then compare to copying 1 byte and next position(pos+1) encoding
    if (numBytes >= 3) {
        numBytes1 = simpleEnc(src, size, pos + 1, &matchPos);
        // if the next position encoding is +2 longer than current position, choose it.
        // this does not guarantee the best optimization, but fairly good optimization with speed.
        if (numBytes1 >= numBytes + 2) {
            numBytes = 1;
            prevFlag = 1;
        }
    }
    return numBytes;
}

struct Ret
{
    uint32_t srcPos, dstPos;
};

void EncodeSlide(FILE* dst_file, std::vector<uint8_t>& src)
{
    Ret r = { 0, 0 };
    uint8_t dst[96];    // 32 codes * 3 bytes maximum
    size_t len = src.size();
    uint32_t validBitCount = 0; //number of valid bits left in "code" byte
    uint32_t currCodeByte = 0;
    WriteU32(dst_file, len);
    while (r.srcPos < len)
    {
        uint32_t numBytes;
        uint32_t matchPos;
        uint32_t srcPosBak;

        numBytes = nintendoEnc(&src[0], len, r.srcPos, &matchPos);
        if (numBytes < 3)
        {
            //straight copy
            dst[r.dstPos] = src[r.srcPos];
            r.dstPos++;
            r.srcPos++;
            //set flag for straight copy
            currCodeByte |= (0x80000000 >> validBitCount);
        }
        else
        {
            //RLE part
            uint32_t dist = r.srcPos - matchPos - 1;
            uint8_t byte1, byte2, byte3;

            if (numBytes >= 0x12)  // 3 byte encoding
            {
                byte1 = 0 | (dist >> 8);
                byte2 = dist & 0xff;
                dst[r.dstPos++] = byte1;
                dst[r.dstPos++] = byte2;
                // maximum runlength for 3 byte encoding
                if (numBytes > 0xff + 0x12)
                    numBytes = 0xff + 0x12;
                byte3 = numBytes - 0x12;
                dst[r.dstPos++] = byte3;
            }
            else  // 2 byte encoding
            {
                byte1 = ((numBytes - 2) << 4) | (dist >> 8);
                byte2 = dist & 0xff;
                dst[r.dstPos++] = byte1;
                dst[r.dstPos++] = byte2;
            }
            r.srcPos += numBytes;
        }
        validBitCount++;
        //write 32 codes
        if (validBitCount == 32)
        {
            WriteU32(dst_file, currCodeByte);
            fwrite(dst, 1, r.dstPos, dst_file);

            srcPosBak = r.srcPos;
            currCodeByte = 0;
            validBitCount = 0;
            r.dstPos = 0;
        }
    }
    if (validBitCount > 0)
    {
        WriteU32(dst_file, currCodeByte);
        fwrite(dst, 1, r.dstPos, dst_file);

        currCodeByte = 0;
        validBitCount = 0;
        r.dstPos = 0;
    }
}

void EncodeRle(FILE* dst_file, std::vector<uint8_t> &src)
{
    uint32_t input_pos = 0;
    uint32_t i;
    size_t search_len;
    uint32_t copy_len = 0;
    uint8_t curr_byte;
    uint8_t next_byte;

    size_t len = src.size();
    while (input_pos < (len - 1)) {
        curr_byte = src[input_pos];
        next_byte = src[input_pos + 1];
        search_len = len - input_pos - 2;
        if (search_len > 127) {
            search_len = 127;
        }
        copy_len = 1;
        if (curr_byte == next_byte) {
            for (i = 1; i < search_len; i++) {
                curr_byte = src[input_pos + i];
                next_byte = src[input_pos + i + 1];
                if (curr_byte != next_byte) {
                    break;
                }
                copy_len++;
            }
            WriteU8(dst_file, copy_len);
            WriteU8(dst_file, src[input_pos]);
            input_pos += copy_len;
        }
        else {
            for (i = 1; i < search_len; i++) {
                curr_byte = src[input_pos + i];
                next_byte = src[input_pos + i + 1];
                if (curr_byte == next_byte) {
                    break;
                }
                copy_len++;
            }
            WriteU8(dst_file, copy_len | 0x80);
            fwrite(&src[input_pos], 1, copy_len, dst_file);
            input_pos += copy_len;
        }
    }
    //Write last byte raw
    WriteU8(dst_file, 1 | 0x80);
    WriteU8(dst_file, src[input_pos]);
}


void EncodeData(FILE* file, uint32_t comptype, std::vector<uint8_t>& data)
{
    WriteU32(file, data.size());
    WriteU32(file, comptype);
    switch (comptype) {
    case 0:
        EncodeNone(file, data);
        break;

    case 1:
        EncodeLZSS(file, data);
        break;

    case 2:
        EncodeSlide(file, data);
        break;


    case 3:
    case 4:
        EncodeSlide(file, data);
        break;

    case 5:
        EncodeRle(file, data);
        break;

    default:
        std::cout << "Invalid compression type " << comptype << "." << std::endl;
        exit(1);
    }
    WriteAlign(file, 2);
}

void WriteFileDataRom(FILE* file)
{
    size_t dircnt = gamedata.filedata.files.size();
    size_t base_ofs = ftell(file);
    std::vector<uint32_t> dir_ofs_all;
    SetSegNameValue(gamedata.filedata.segname, base_ofs, false);
    WriteU32(file, dircnt);
    for (size_t i = 0; i < dircnt; i++) {
        WriteU32(file, 0);
    }
    for (size_t i = 0; i < dircnt; i++) {
        size_t dir_ofs = ftell(file);
        size_t filecnt = gamedata.filedata.files[i].size();
        std::vector<uint32_t> dir_file_ofs;
        dir_ofs_all.push_back(dir_ofs - base_ofs);
        WriteU32(file, filecnt);
        for (size_t j = 0; j < filecnt; j++) {
            WriteU32(file, 0);
        }
        for (size_t j = 0; j < filecnt; j++) {
            FileData& filedata = gamedata.filedata.files[i][j];
            dir_file_ofs.push_back(ftell(file) - dir_ofs);
            EncodeData(file, filedata.comp_type, filedata.data);
        }
        for (size_t j = 0; j < filecnt; j++) {
            WriteU32At(file, dir_file_ofs[j], dir_ofs + (j * 4) + 4);
        }
    }
    for (size_t i = 0; i < dircnt; i++) {
        WriteU32At(file, dir_ofs_all[i], base_ofs + (i * 4) + 4);
    }
    WriteAlign(file, 16);
}

void WriteMessDataRom(FILE* file, MessDataSegment& messdata)
{
    messdata.romaddr = ftell(file);
    SetSegNameValue(messdata.segname, messdata.romaddr, false);
    if (messdata.new_format) {
        std::vector<uint32_t> dir_ofs;
        size_t base_ofs = ftell(file);
        size_t dircnt = messdata.mess_dir_all.size();
        WriteU32(file, dircnt);
        for (size_t i = 0; i < dircnt; i++) {
            WriteU32(file, 0);
        }
        for (size_t i = 0; i < dircnt; i++) {
            dir_ofs.push_back(ftell(file) - base_ofs);
            EncodeData(file, 1, messdata.mess_dir_all[i].data);
        }
        for (size_t i = 0; i < dircnt; i++) {
            WriteU32At(file, dir_ofs[i], base_ofs + (i * 4) + 4);
        }
    } else {
        WriteRawBuffer(file, messdata.full_data);
    }
    WriteAlign(file, 16);
}

void WriteHvqDataRom(FILE* file)
{
    size_t bgcnt = gamedata.hvqdata.hvq_data.size();
    size_t base_ofs = ftell(file);
    gamedata.hvqdata.romaddr = base_ofs;
    SetSegNameValue(gamedata.hvqdata.segname, base_ofs, false);
    std::vector<uint32_t> bg_ofs;
    WriteU32(file, bgcnt+1);
    for (size_t i = 0; i < bgcnt+1; i++) {
        WriteU32(file, 0);
    }
    for (size_t i = 0; i < bgcnt; i++) {
        bg_ofs.push_back(ftell(file) - base_ofs);
        WriteRawBuffer(file, gamedata.hvqdata.hvq_data[i]);
        WriteAlign(file, 2);
    }
    bg_ofs.push_back(ftell(file) - base_ofs);
    for (size_t i = 0; i < bgcnt+1; i++) {
        WriteU32At(file, bg_ofs[i], base_ofs + (i * 4) + 4);
    }
    WriteAlign(file, 16);
}

void WriteBgAnimDataRom(FILE* file)
{
    size_t base_ofs = ftell(file);
    gamedata.bganimdata.romaddr = base_ofs;
    SetSegNameValue(gamedata.bganimdata.segname, base_ofs, false);
    size_t count = gamedata.bganimdata.bganim_data.size();
    std::vector<uint32_t> data_ofs;
    WriteU32(file, count + 1);
    for (size_t i = 0; i < count + 1; i++) {
        WriteU32(file, 0);
    }
    for (size_t i = 0; i < count; i++) {
        data_ofs.push_back(ftell(file) - base_ofs);
        WriteRawBuffer(file, gamedata.bganimdata.bganim_data[i]);
        WriteAlign(file, 2);
    }
    data_ofs.push_back(ftell(file) - base_ofs);
    for (size_t i = 0; i < count + 1; i++) {
        WriteU32At(file, data_ofs[i], base_ofs + (i * 4) + 4);
    }
    WriteAlign(file, 16);
}

void WriteMusBankRom(FILE* file, MusBankSegment& musbank)
{
    musbank.romaddr = ftell(file);
    SetSegNameValue(musbank.segname, musbank.romaddr, false);
    
    // Generate musbank header
    uint32_t count = musbank.libaudioseg.seqsegs.size();
    uint32_t soundbanksize = musbank.libaudioseg.soundbankseg.data.size();

    WriteRawBuffer(file, musbank.revision);
    if (musbank.new_format) {
       WriteU32(file, count);
       WriteRawBuffer(file, musbank.unkdata);

       uint32_t headersize = 80 + 16 * count;
       uint32_t offset = headersize;
       for (auto& seqseg : musbank.libaudioseg.seqsegs) {
          uint32_t seqsize = seqseg.data.size();
          WriteU8(file, seqseg.unk0);
          WriteU8(file, seqseg.unk1);
          WriteU8(file, seqseg.bank);
          WriteU8(file, 0);
          WriteU32(file, 0x07000000); // unused
          if (seqseg.id == -1) { // original data
             seqseg.romaddr = offset;
             WriteU32(file, offset);
             WriteU32(file, seqseg.data.size());
             offset += seqseg.data.size();
             offset = BIT_ALIGN(offset, 8);
          }
          else {
             // Write copy sequence data
             auto& copyseqseg = musbank.libaudioseg.seqsegs[seqseg.id];
             WriteU32(file, copyseqseg.romaddr);
             WriteU32(file, copyseqseg.data.size());
          }
       }

       WriteU32(file, offset);
       WriteU32(file, soundbanksize);
       WriteU32(file, offset + soundbanksize);
       WriteU32(file, musbank.libaudioseg.wavetableseg.data.size());
    }
    else {
       WriteU16(file, count);
       uint32_t headersize = 4 + count * 24;
       headersize = BIT_ALIGN(headersize, 16); // padding
       uint32_t offset = soundbanksize + headersize + musbank.libaudioseg.wavetableseg.data.size();
       for (auto& seqseg : musbank.libaudioseg.seqsegs) {
          uint32_t seqsize = seqseg.data.size();
          WriteU32(file, offset);
          WriteU32(file, seqsize);
          seqsize = BIT_ALIGN(seqsize, 8); // Don't forget about the padding
          offset += seqsize;
       }
       for (auto& seqseg : musbank.libaudioseg.seqsegs) {
          uint32_t seqsize = seqseg.data.size();
          WriteU8(file, seqseg.bank);
          WriteU8(file, 0x7F); // unused
          WriteU8(file, 0xFF); // unused
          WriteU8(file, 0xFF); // unused

          WriteU32(file, headersize);
          WriteU32(file, soundbanksize);
          WriteU32(file, headersize + soundbanksize);
       }
    }
    WriteAlignFF(file, 16);

    // midi order is first for new_format
    if (musbank.new_format) {
       for (auto& seq : musbank.libaudioseg.seqsegs) {
          if (seq.id == -1) {
             WriteRawBuffer(file, seq.data);
             WriteAlign(file, 8);
          }
       }
    }
    WriteRawBuffer(file, musbank.libaudioseg.soundbankseg.data);
    WriteRawBuffer(file, musbank.libaudioseg.wavetableseg.data);
    if (!musbank.new_format) {
       for (auto& seq : musbank.libaudioseg.seqsegs) {
          WriteRawBuffer(file, seq.data);
          WriteAlign(file, 8); // Already aligned in mp1, this is for custom data
       }
    }
    WriteAlign(file, 16);
    SetSegNameValue(musbank.segname, ftell(file), true);
}

void WriteSfxBankRom(FILE* file, SfxBankSegment& sfxbank)
{
    sfxbank.romaddr = ftell(file);
    SetSegNameValue(sfxbank.segname, sfxbank.romaddr, false);
    WriteRawBuffer(file, sfxbank.data);
    WriteAlign(file, 16);
    SetSegNameValue(sfxbank.segname, ftell(file), true);
}

void WriteFxDataRom(FILE* file)
{
    gamedata.fxdata.romaddr = ftell(file);
    SetSegNameValue(gamedata.fxdata.segname, gamedata.fxdata.romaddr, false);
    WriteRawBuffer(file, gamedata.fxdata.data);
    WriteAlign(file, 16);
    SetSegNameValue(gamedata.fxdata.segname, ftell(file), true);
}

void WriteNewSegRefs(FILE* file)
{
    for (size_t i = 0; i < gamedata.segrefs.size(); i++) {
        SegRef& segref = gamedata.segrefs[i];
        uint32_t hi_dst = segref.hi;
        uint32_t lo_dst = segref.lo;
        uint32_t value = segref.value;
        uint16_t lo = value & 0xFFFF;
        uint16_t hi = (value >> 16);
        if (lo > 0x8000) {
            hi++;
        }
        WriteU16At(file, hi, hi_dst);
        WriteU16At(file, lo, lo_dst);
    }
}
void WriteRom(std::string output)
{
    FILE* file = fopen(output.c_str(), "wb");
    if (!file) {
        std::cout << "Failed to open " << output << " for writing." << std::endl;
        exit(1);
    }
    size_t initial_size = gamedata.filedata.romaddr;
    //Copy Initial Section of ROM
    fwrite(&rom_data[0], 1, initial_size, file);
    WriteFileDataRom(file);
    for (size_t i = 0; i < gamedata.messdata_all.size(); i++) {
        WriteMessDataRom(file, gamedata.messdata_all[i]);
    }
    
    WriteHvqDataRom(file);
    if (game_id == "mp2") {
        WriteBgAnimDataRom(file);
    }
    for (size_t i = 0; i < gamedata.musbanks.size(); i++) {
        WriteMusBankRom(file, gamedata.musbanks[i]);
    }
    for (size_t i = 0; i < gamedata.sfxbanks.size(); i++) {
        WriteSfxBankRom(file, gamedata.sfxbanks[i]);
    }
    WriteFxDataRom(file);
    WriteNewSegRefs(file);
    std::string romid = ReadRomGameID();
    //Wrong Save Type Hang/Initialization Fix
    if (romid == "NMVE") {
        WriteU32At(file, 0, 0xCEC0);
        WriteU32At(file, 0, 0x50950);
    } else if (romid == "NMVP") {
        WriteU32At(file, 0, 0xCEE0);
        WriteU32At(file, 0, 0x50990);
    } else if (romid == "NMVJ") {
        WriteU32At(file, 0, 0xCEC0);
        WriteU32At(file, 0, 0x507EC);
    }
    fclose(file);
}

#include "crc.inc"

void RebuildRom(std::string indir, std::string output)
{
    ParseRomData(indir + "/romdata.xml");
    WriteRom(output);
    fix_crc(output.c_str());
}
int main(int argc, char **argv)
{
    bool build_rom = false;
    size_t last_opt = 1;
    desc_path = "gameconfig";
    for (int i = 1; i < argc; i++) {
        std::string option = argv[i];
        if (option[0] != '-') {
            last_opt = i;
            break;
        }
        if (option == "-h" || option == "--help") {
            PrintHelp(argv[0]);
            exit(0);
        }
        if (option == "-b" || option == "--build") {
            build_rom = true;
        } else if (option == "-d" || option == "--desc") {
            if (++i >= argc) {
                std::cout << "Missing argument for option " << option << "." << std::endl;
                PrintHelp(argv[0]);
                exit(1);
            }
            desc_path = argv[i];
        } else if (option == "-a" || option == "--base") {
            if (++i >= argc) {
                std::cout << "Missing argument for option " << option << "." << std::endl;
                PrintHelp(argv[0]);
                exit(1);
            }
            if (rom_data.size() != 0) {
                std::cout << "Multiple Base ROM Arguments disallowed" << std::endl;
                PrintHelp(argv[0]);
                exit(1);
            }
            LoadROM(argv[i]);
        } else {
            std::cout << "Invalid option " << option <<  "." << std::endl;
            PrintHelp(argv[0]);
            exit(1);
        }
    }
    if (rom_data.size() == 0) {
        std::cout << "Missing Base ROM." << std::endl;
        PrintHelp(argv[0]);
        exit(1);
    }
    ReadGameDesc(ReadRomGameID());
    if (!build_rom) {
        if (argc - last_opt != 1) {
            std::cout << "Invalid arguments after flags." << std::endl;
            PrintHelp(argv[0]);
            exit(1);
        }
        ExtractROM(argv[last_opt]);
    } else {
        if (argc - last_opt != 2) {
            std::cout << "Invalid arguments after flags." << std::endl;
            PrintHelp(argv[0]);
            exit(1);
        }
        RebuildRom(argv[last_opt], argv[last_opt + 1]);
    }
}