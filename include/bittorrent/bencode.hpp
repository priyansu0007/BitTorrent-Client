#pragma once

#include <variant>
#include <string>
#include <string_view>
#include <optional>
#include <cstdint>
#include <cstddef>
#include <map>
#include <vector>

namespace bittorrent{
    struct BValue;
    using BList=std::vector<BValue>;    //similar to typedef(rename vector of BValue to BList)
    using BDict=std::map<std::string,BValue>;

    struct BValue{
        std::variant<int64_t,std::string,BList,BDict> data;

        //check the type of data
        bool is_int() const{return std::holds_alternative<int64_t>(data);}
        bool is_string() const{return std::holds_alternative<std::string>(data);}
        bool is_list() const{return std::holds_alternative<BList>(data);}
        bool is_dict() const{return std::holds_alternative<BDict>(data);}

        //access the data
        int64_t as_int() const{return std::get<int64_t>(data);}
        const std::string& as_string() const{return std::get<std::string>(data);}
        const BList& as_list() const{return std::get<BList>(data);}
        const BDict& as_dict() const{return std::get<BDict>(data);}
    };

    class BencodeParser{
        private:
            std::string_view buffer_;
            size_t cursor_{0}; //to bookmark the part where we are reading
            std::optional<std::string_view> raw_info_slice_; //dict mignt not be present

            char peek() const; //check byte at cursor
            char get();        //get byte at cursor
            bool has_more() const; //checks if cursor < buffer.size()

            //parsing functions
            BValue parse_int();
            BValue parse_string();
            BValue parse_list();
            BValue parse_dict();

            
        public:
            explicit BencodeParser(std::string_view raw_data); //to avoid type conversion
            BValue parse();
            std::string_view get_raw_info_dict() const;
            
    };
}

