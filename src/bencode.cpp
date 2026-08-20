#include "bittorrent/bencode.hpp"
#include <charconv>

namespace bittorrent{
    BencodeParser::BencodeParser(std::string_view raw_data)
        : buffer_(raw_data), cursor_(0) {}
    
        char BencodeParser::peek() const{
            if(!has_more()) throw std::runtime_error("Unexpected eof while peeking");
            return buffer_[cursor_];
        }
        char BencodeParser::get(){
            if(!has_more())throw std::runtime_error("BencodeParser: Unexpected eof while reading byte");
            return buffer_[cursor_++];
        }
        bool BencodeParser::has_more() const {return cursor_<buffer_.size();}

        BValue BencodeParser::parse(){
            char c=peek();
            if(c=='i')return parse_int();
            else if(c=='l') return parse_list();
            else if(c=='d') return parse_dict();
            else if(c>='0' && c<='9')return parse_string();

            throw std::runtime_error("BencodeParser: invalid token '"+std::string(1,c)+"' at byte "+std::to_string(cursor_));
        }

        BValue BencodeParser:: parse_int(){
            get();
            size_t start=cursor_;
            while(has_more() && peek()!='e')cursor_++;
            if(!has_more())throw std::runtime_error("BencodeParser: Unterminated integer");
            size_t end=cursor_;
            get();
            std::string_view num_str=buffer_.substr(start,end-start);

            //check edge cases
            if(num_str.empty())throw std::runtime_error("BencodeParser: empty integer found");
            if(num_str=="-0") throw std::runtime_error("BencodeParser: negative zero i-0e");
            if(num_str.size()>1 && num_str[0]=='0')throw std::runtime_error("BencodeParser: leading zeroes found ");
            if(num_str.size()>2 && num_str[0]=='-' && num_str[1]=='0')throw std::runtime_error("BencodeParser: leading zeroes found in negative integer");

            int64_t value=0;
            auto [ptr,ec]=std::from_chars(num_str.data(),num_str.data()+num_str.size(),value);//convert the num_str to integer and store in value

            if(ec!=std::errc())throw std::runtime_error("Failed to parse the integer");

            return BValue{value};


        }
}