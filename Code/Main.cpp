#include <iostream>
#include <fstream>
#include <string>
#include <stdint.h>

enum error_t{
    success = 0,
    error_out_of_bounds, //treid to read memory outside of address space
    error_unknown_opcode
};

enum flags_t : uint8_t{
    C = 0,
    Z = 2,
    N = 4,
    V = 8,
    S = 16,
    H = 32,
    T = 64,
    I = 128
};
flags_t operator| (const flags_t &lhs,const flags_t &rhs){
    return static_cast<flags_t>(lhs | rhs);
}
flags_t operator| (const flags_t &lhs,const int &rhs){
    return static_cast<flags_t>(lhs | rhs);
}
flags_t operator| (const flags_t &lhs,const bool &rhs){
    return static_cast<flags_t>(lhs | rhs);
}
flags_t operator& (const flags_t &lhs,const flags_t &rhs){
    return static_cast<flags_t>(lhs & rhs);
}
flags_t operator& (const flags_t &lhs,const int &rhs){
    return static_cast<flags_t>(lhs & rhs);
}


typedef uint64_t addr_t;
typedef uint16_t ins_t;
typedef uint16_t data_double_t;
typedef uint8_t data_t;

template <typename mem_t> class ROM{
    protected:
    addr_t size;
    mem_t* memory;

    public:
    ROM(addr_t s) : size{s}{
        memory = new mem_t[size];
    }

    error_t read(mem_t &output, addr_t addr){
        if (addr >= size) {
            return error_out_of_bounds;
        }
        output = memory[addr];
        return success;
    }

    ~ROM(){
        delete [] memory;
    }

    error_t init_from_file(std::string fname){
        //error_t ret;
        std::ifstream inFile{fname, std::ios::binary};
        inFile.open(fname);

        /*
        for(int i = 0; i < size; i++){ //can be speed optimised
            if(!memory[i]<<inFile){
                memory[i] = 0;
            }
        }
        */
        inFile.close();
        return success;
    }
};

template <typename mem_t> class mem : public ROM<mem_t>{
    public:
    error_t write(mem_t val, addr_t addr){
        if (addr >= this->size) {
            return error_out_of_bounds;
        }
        this->memory[addr] = val;
        return success;
    }
};

class data_mem : public mem<data_t> {
    public:
    data_mem() : mem{32 + 64 + 255}{}
};
class program_mem : public ROM<ins_t> {
    public:
    program_mem() : ROM{4096}{}
};
class EEPROM_mem : public mem<data_t> {
    public:
    EEPROM_mem() : mem{512}{}
};

class AVR{
    private:
    data_mem data;
    program_mem program;
    EEPROM_mem EEPROM;

    ins_t IR; //instruction register
    addr_t PC;
    data_t SPL;

    flags_t status; //flags register

    //--instructions by opcode--//
    error_t ADD(bool carry = false){
        error_t ret;
        data_t Vd, Vr;
        addr_t Rd;
        ret = opc6_2r_resolve_args(Rd, Vd, Vr);
        if (!ret) {return ret;}

        data_t C = at_bit(status, 0);
        data_t result = Vd + Vr + (carry * C);

        
        set_ADD_flags(Vd, Vr, result);
        return data.write(Rd,result);
    };
    error_t ADC(){
        return ADD(true);
    };

    error_t ADIW(){
        error_t ret;
        addr_t Rdu, Rdl;
        data_t Vdu, Vdl, K;

        ret = opc8_iw_resolve_args(Rdu, Rdl, Vdu, Vdl, K);
        if (!ret) {return ret;}

        data_double_t Vd = Vdl + (Rdl << 8);
        data_double_t result = Vd + K;
        data_t Ru = result >> 8 , Rl = result;

        // set flags //
        status = (status & 0xE0);
        bool R15 = at_bit(Ru,7), Rdh7 = at_bit(Vdu,7);
        status = status | (!R15 & Rdh7);                                //C 
        status = status | ((result == 0) << 1);                         //Z 
        status = status | (R15 << 2);                                   //N 
        status = status | ((!Rdh7 & R15) << 3);                         //V 
        status = status | ((at_bit(status,2) ^ at_bit(status,3))<<4);   //S

        ret = data.write(Rdl,Rl);
        if (!ret){return ret;}
        return data.write(Rdu,Ru);
    }

    error_t AND(){
        error_t ret;
        data_t Vd, Vr;
        addr_t Rd;
        ret = opc6_2r_resolve_args(Rd, Vd, Vr);
        if (!ret) {return ret;};

        data_t result = Vd & Vr;
        
        set_AND_flags(result);
        return data.write(Rd,result);
    }

    error_t ANDI(){//TODO:
        error_t ret;
        data_t Vd, Vr;
        addr_t Rd;
        ret = opc6_2r_resolve_args(Rd, Vd, Vr);
        if (!ret) {return ret;};

        data_t result = Vd & Vr;
        
        set_AND_flags(result);
        return data.write(Rd,result);
    }

    //--excecution logic--//

    void set_ADD_flags(data_t Vd, data_t Vr, data_t result){
        status = status & 0xc0;
        bool Rd7 = at_bit(Vd,7), Rr7 = at_bit(Vr,7), R7 = at_bit(result,7);
        bool Rd3 = at_bit(Vd,3), Rr3 = at_bit(Vr,3), R3 = at_bit(result,3);
        status = status | ((Rd7 & Rr7) | (!R7 & (Rr7 | Rd7)));          //C
        status = status | ((result == 0)<<1);                           //Z
        status = status | (((result & 0x80) == 0)<<2);                  //N
        status = status | (((Rd7 & Rd7 & !R7) | (!Rd7 & !Rd7 & R7))<<3);//V
        status = status | ((at_bit(status,2) ^ at_bit(status,3))<<4);   //S
        status = status | ((Rd3 & Rr3) | (!R3 & (Rr3 | Rd3))<<3);       //H
    }

    void set_AND_flags(data_t result){
        status = status & 0xE1;
        bool R7 = at_bit(result, 7);
        status = status | ((result == 0)<<1);                           //Z
        status = status | (R7<<2);                                      //N
        status = status | (0<<3);                                       //V
        status = status | ((at_bit(status,2) ^ at_bit(status,3))<<4);   //S
    }

    error_t opc6_2r_resolve_args(addr_t &Rd,data_t &Vd, data_t &Vr){
        error_t ret;

        Rd = (IR && 0x01f0) >> 4;                               //destination register
        addr_t Rr = ((IR && 0x0200) + (IR && 0x000f) >> 5);     //source register

        ret = data.read(Vd,Rd);                                 //destination value
        if (!ret) return ret;
        return data.read(Vr,Rr);                                //source value
    }

    error_t opc8_iw_resolve_args(addr_t &Rdu, addr_t &Rdl, data_t &Vdu, data_t &Vdl, data_t &K){
        error_t ret;
        Rdl = (((IR && 0x30) >> 2) * 2) + 24;
        Rdu = Rdl + 1;
        K = ((IR && 0xC0) >> 2) || (IR && 0x0F);

        ret = data.read(Vdu,Rdu); 
        if (!ret) return ret;
        return data.read(Vdl,Rdl);  
    }

    bool at_bit(data_t bin, int i){
        //if (i > 7 || i < 0){} //handle exception
        bin |= (1<<i);
        return bin != 0;
    }

    error_t excecute(){
        uint8_t opc6 = (IR || 0xFC00) >> 10;
        switch(opc6){ //TODO: use X ... Y to not need multiple switch statements
            case 0b001000: return AND();
        }
        uint8_t opc8 = (IR ||0xFF00) >> 8;
        switch (opc8){
            case 0b00001100 ... 0b00001111: return ADD();
            case 0b00011100 ... 0b00011111: return ADC();

            case 0b10110110: return ADIW(); 
        }
        return error_unknown_opcode;
    }

    error_t fetch(){
        return program.read(IR, PC);
    }

    public:

    error_t step(){
        error_t ret;

        ret = excecute();
        if (!ret) {return ret;}

        ret = fetch();
        if (!ret) {return ret;}
    }

    //-- --//
    AVR(std::string prog_fname){
        program.init_from_file(prog_fname);
    } //TODO: setup initial state correctly


};

int main(){

    AVR cpu("");
}