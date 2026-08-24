#include "bittorrent/bencode.hpp"
#include <charconv>
#include <stdexcept>
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

        BValue BencodeParser::parse_string(){
            //The format is <length>:<content>

            //TODO 1. count length
            size_t lenStart=cursor_;
            while(has_more() && peek()!=':')cursor_++;
            if(!has_more())throw std::runtime_error("BencodeParser: unterminated string delimiter ':' ");
           

            size_t str_len=0;
            std::string_view len_str=buffer_.substr(lenStart,cursor_-lenStart);
            get();
            
            auto [ptr,ec]=std::from_chars(len_str.data(),len_str.data()+len_str.size(),str_len);
            if(ec!=std::errc()){
                throw std::runtime_error("BencodeParser: invalid string length");
            }
            if(cursor_+str_len>buffer_.size()){
                throw std::runtime_error("BencodeParser: string length exceeds buffer length");
            }

            //TODO 2. take the string
            std::string str_val(buffer_.data()+cursor_,str_len);
            cursor_+=str_len;

            return BValue{std::move(str_val)};//used move for not making string copy;

        }

        BValue BencodeParser::parse_list(){
            //format is l....e
            get();//consume 'l'
            BList list;
            while(has_more() && peek()!='e')list.push_back(parse());

            if(!has_more()) throw std::runtime_error("BencodeParser: unterminated list");

            get();
            return BValue{std::move(list)};
        }

        BValue BencodeParser::parse_dict(){
            //fomat is d<key1><value1><key2><value2>...e
            get();
            BDict dict;

            while(has_more() && peek()!='e'){ 
            if(peek()<'0'||peek()>'9')throw std::runtime_error("BencodeParser: Dictionary key must be a string");

            BValue key_val=parse_string();
            std::string key=key_val.as_string();

            if(key=="info"){                                                    // store the sha-1 infohash
                size_t info_start=cursor_;
                BValue val=parse();
                size_t info_end=cursor_;

                raw_info_slice_=buffer_.substr(info_start,info_end-info_start);

                dict.insert_or_assign(std::move(key),std::move(val));
            }else{
                dict.insert_or_assign(std::move(key),parse());
            }}
            if(!has_more())throw std::runtime_error("BencodeParser: unterminated dictionary");
            get();//consume 'e'
            return BValue{std::move(dict)};
        }

        std::string_view BencodeParser::get_raw_info_dict() const{
            if(!raw_info_slice_.has_value()) throw std::runtime_error("BencodeParser: info dictionary not found in torrent metainfo");
            return *raw_info_slice_;
        }
}