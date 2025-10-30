// Auto-generated 2025-03-17T14:34:12 by /Users/schuemann/Development/c8/chiplet/defs/clarke.py

#pragma once

#include <cstdint>
#include <string>

namespace emu {

namespace saturn {
enum VariableType {
    vt_field,
    vt_regpair4,
    vt_regpair4rev,
    vt_regpair8,
    vt_regpair8split,
    vt_regpair12,
    vt_reg,
    vt_mrpair,
    vt_daregpair,
    vt_daregpairrev,
    vt_tempreg,
    vt_maxtables,
    vt_const = vt_maxtables,
    vt_nzconst,
    vt_varconst,
    vt_pcofs,
    vt_hwflags,
    vt_none
};
}

class SaturnDecoder {
public:
    struct InstructionInfo {
        const char* fmt;
        uint32_t opcode;
        int8_t opcodeSize;
        uint8_t numVars;
        uint8_t varSizes[3];
        uint8_t varOffsets[3];
        uint8_t varTypes[3];
        uint8_t varArgs[3];
    };
    
    
    enum OpcodeId
    {
        Opc_809_add_a_p_1_c,
        Opc_cx_add_a_x_regpair12,
        Opc_akx_add_k_field_x_regpair12,
        Opc_818fxi_add_a_i_nzconst_4_x_reg,
        Opc_818txi_add_t_field_i_nzconst_4_x_reg,
        Opc_16x_add_a_x_nzconst_4_d0,
        Opc_17x_add_a_x_nzconst_4_d1,
        Opc_0efy_and_a_y_regpair8,
        Opc_0exy_and_x_field_y_regpair8,
        Opc_8086xyy_brbc_x_const_4_a_yy_pcofs_5,
        Opc_808axyy_brbc_x_const_4_c_yy_pcofs_5,
        Opc_86xyy_brbc_x_const_4_st_yy_pcofs_3,
        Opc_83zyy_brbc_z_hwflags_yy_pcofs_3,
        Opc_8087xyy_brbs_x_const_4_a_yy_pcofs_5,
        Opc_808bxyy_brbs_x_const_4_c_yy_pcofs_5,
        Opc_87xyy_brbs_x_const_4_st_yy_pcofs_3,
        Opc_8086x00_retbc_x_const_4_a,
        Opc_808ax00_retbc_x_const_4_c,
        Opc_86x00_retbc_x_const_4_st,
        Opc_83z00_retbc_z_hwflags,
        Opc_8087x00_retbs_x_const_4_a,
        Opc_808bx00_retbs_x_const_4_c,
        Opc_87x00_retbs_x_const_4_st,
        Opc_5xx_brcc_xx_pcofs_1,
        Opc_4xx_brcs_xx_pcofs_1,
        Opc_500_retcc,
        Opc_400_retcs,
        Opc_89xyy_breq_1_p_x_const_4_yy_pcofs_3,
        Opc_88xyy_brne_1_p_x_const_4_yy_pcofs_3,
        Opc_89x00_reteq_1_p_x_const_4,
        Opc_88x00_retne_1_p_x_const_4,
        Opc_8auyy_breq_a_u_regpair4_0_yy_pcofs_3,
        Opc_9tuyy_breq_t_field_u_regpair4_0_yy_pcofs_3,
        Opc_8auyy_brne_a_u_regpair4_4_yy_pcofs_3,
        Opc_9tuyy_brne_t_field_u_regpair4_4_yy_pcofs_3,
        Opc_8auyy_brz_a_u_reg_8_yy_pcofs_3,
        Opc_9tuyy_brz_t_field_u_reg_8_yy_pcofs_3,
        Opc_8auyy_brnz_a_u_reg_12_yy_pcofs_3,
        Opc_9tuyy_brnz_t_field_u_reg_12_yy_pcofs_3,
        Opc_8buyy_brgt_a_u_regpair4_0_yy_pcofs_3,
        Opc_9tuyy_brgt_t_field_8_u_regpair4_0_yy_pcofs_3,
        Opc_8buyy_brlt_a_u_regpair4_4_yy_pcofs_3,
        Opc_9tuyy_brlt_t_field_8_u_regpair4_4_yy_pcofs_3,
        Opc_8buyy_brge_a_u_regpair4_8_yy_pcofs_3,
        Opc_9tuyy_brge_t_field_8_u_regpair4_8_yy_pcofs_3,
        Opc_8buyy_brle_a_u_regpair4_12_yy_pcofs_3,
        Opc_9tuyy_brle_t_field_8_u_regpair4_12_yy_pcofs_3,
        Opc_8au00_reteq_a_u_regpair4_0,
        Opc_9tu00_reteq_t_field_u_regpair4_0,
        Opc_8au00_retne_a_u_regpair4_4,
        Opc_9tu00_retne_t_field_u_regpair4_4,
        Opc_8au00_retz_a_u_reg_8,
        Opc_9tu00_retz_t_field_u_reg_8,
        Opc_8au00_retnz_a_u_reg_12,
        Opc_9tu00_retnz_t_field_u_reg_12,
        Opc_8bu00_retgt_a_u_regpair4_0,
        Opc_9tu00_retgt_t_field_8_u_regpair4_0,
        Opc_8bu00_retlt_a_u_regpair4_4,
        Opc_9tu00_retlt_t_field_8_u_regpair4_4,
        Opc_8bu00_retge_a_u_regpair4_8,
        Opc_9tu00_retge_t_field_8_u_regpair4_8,
        Opc_8bu00_retle_a_u_regpair4_12,
        Opc_9tu00_retle_t_field_8_u_regpair4_12,
        Opc_8083_buscb,
        Opc_80b_buscc,
        Opc_808d_buscd,
        Opc_804_uncnfg,
        Opc_805_config,
        Opc_807_shutdn,
        Opc_80a_reset,
        Opc_80e_sreq,
        Opc_7xxx_call_3_xxx_pcofs_4,
        Opc_8exxxx_call_4_xxxx_pcofs_6,
        Opc_8fxxxxx_call_a_xxxxx_const_20,
        Opc_dt_clr_a_t_reg,
        Opc_apt_clr_p_field_8_t_reg,
        Opc_8084x_clrb_x_const_4_a,
        Opc_8088x_clrb_x_const_4_c,
        Opc_84x_clrb_x_const_4_st,
        Opc_82x_clrb_x_hwflags,
        Opc_08_clr_x_st,
        Opc_0d_dec_1_p,
        Opc_cw_dec_a_w_reg_12,
        Opc_akw_dec_k_field_w_reg_12,
        Opc_802_in_4_a,
        Opc_803_in_4_c,
        Opc_0c_inc_1_p,
        Opc_eu_inc_a_u_reg_4,
        Opc_bku_inc_k_field_u_reg_4,
        Opc_808f_intoff,
        Opc_8080_inton,
        Opc_80810_rsi,
        Opc_808c_jump_a_a,
        Opc_808e_jump_a_c,
        Opc_81b2_jump_a_a,
        Opc_81b3_jump_a_c,
        Opc_81b4_move_a_pc_a,
        Opc_81b5_move_a_pc_c,
        Opc_81b6_swap_a_a_pc,
        Opc_81b7_swap_a_c_pc,
        Opc_6xxx_jump_3_xxx_pcofs_1,
        Opc_8cxxxx_jump_4_xxxx_pcofs_2,
        Opc_8dxxxxx_jump_a_xxxxx_const_20,
        Opc_806_move_a_id_c,
        Opc_dz_move_a_z_regpair8_4,
        Opc_apz_move_p_field_8_z_regpair8_4,
        Opc_dz_swap_a_z_regpair4rev_12,
        Opc_apz_swap_p_field_8_z_regpair4rev_12,
        Opc_14x_move_a_x_mrpair_0,
        Opc_14x_move_b_x_mrpair_8,
        Opc_15xt_move_t_field_x_mrpair_0,
        Opc_15xi_move_i_nzconst_4_x_mrpair_8,
        Opc_13x_move_a_x_daregpair_0,
        Opc_13x_move_4_x_daregpair_8,
        Opc_13x_swap_a_x_daregpair_2,
        Opc_13x_swap_4_x_daregpair_10,
        Opc_10x_move_w_a_x_tempreg_0,
        Opc_12x_swap_w_a_x_tempreg_0,
        Opc_10x_move_w_c_x_tempreg_8,
        Opc_12x_swap_w_c_x_tempreg_8,
        Opc_11x_move_w_x_tempreg_0_a,
        Opc_11x_move_w_x_tempreg_8_c,
        Opc_81af0x_move_a_a_x_tempreg_0,
        Opc_81af2x_swap_a_a_x_tempreg_0,
        Opc_81af0x_move_a_c_x_tempreg_8,
        Opc_81af2x_swap_a_c_x_tempreg_8,
        Opc_81af1x_move_a_x_tempreg_0_a,
        Opc_81af1x_move_a_x_tempreg_8_c,
        Opc_81at0x_move_t_field_a_x_tempreg_0,
        Opc_81at2x_swap_t_field_a_x_tempreg_0,
        Opc_81at0x_move_t_field_c_x_tempreg_8,
        Opc_81at2x_swap_t_field_c_x_tempreg_8,
        Opc_81at1x_move_t_field_x_tempreg_0_a,
        Opc_81at1x_move_t_field_x_tempreg_8_c,
        Opc_3ix_move_p_i_nzconst_4_x_varconst_i_c,
        Opc_8082ix_move_p_i_nzconst_4_x_varconst_i_a,
        Opc_19xx_move_2_xx_const_8_d0,
        Opc_1axxxx_move_4_xxxx_const_16_d0,
        Opc_1bxxxxx_move_5_xxxxx_const_20_d0,
        Opc_1dxx_move_2_xx_const_8_d1,
        Opc_1exxxx_move_4_xxxx_const_16_d1,
        Opc_1fxxxxx_move_5_xxxxx_const_20_d1,
        Opc_2x_move_1_x_const_4_p,
        Opc_80cx_move_1_p_c_x_const_4,
        Opc_80dx_move_1_c_x_const_4_p,
        Opc_09_move_x_st_c,
        Opc_0a_move_x_c_st,
        Opc_0b_swap_x_c_st,
        Opc_fv_neg_a_v_reg_8,
        Opc_brv_neg_r_field_8_v_reg_8,
        Opc_420_nop3,
        Opc_6300_nop4,
        Opc_64000_nop5,
        Opc_fv_not_a_v_reg_12,
        Opc_brv_not_r_field_8_v_reg_12,
        Opc_0efy_or_a_y_regpair8_8,
        Opc_0exy_or_x_field_y_regpair8_8,
        Opc_800_out_s_c,
        Opc_801_out_x_c,
        Opc_06_push_a_c,
        Opc_07_pop_a_c,
        Opc_01_ret,
        Opc_02_retsetc,
        Opc_03_retclrc,
        Opc_0f_reti,
        Opc_00_retsetxm,
        Opc_81x_rln_w_x_reg_0,
        Opc_81x_rrn_w_x_reg_4,
        Opc_8085x_setb_x_const_4_a,
        Opc_8089x_setb_x_const_4_c,
        Opc_85x_setb_x_const_4_st,
        Opc_05_setdec,
        Opc_04_sethex,
        Opc_fw_sln_a_w_reg,
        Opc_brw_sln_r_field_8_w_reg,
        Opc_fw_srn_a_w_reg_4,
        Opc_brw_srn_r_field_8_w_reg_4,
        Opc_81w_srb_w_w_reg_12,
        Opc_819fw_srb_a_w_reg,
        Opc_819rw_srb_r_field_w_reg,
        Opc_bty_sub_t_field_y_regpair8split,
        Opc_ey_sub_a_y_regpair8split,
        Opc_bty_subn_t_field_y_regpair4rev_12,
        Opc_ey_subn_a_y_regpair4rev_12,
        Opc_818fxi_sub_a_i_nzconst_4_x_reg_8,
        Opc_818txi_sub_t_field_i_nzconst_4_x_reg_8,
        Opc_18x_sub_a_x_nzconst_4_d0,
        Opc_1cx_sub_a_x_nzconst_4_d1,
        Opc_80fx_swap_1_p_c_x_const_4,
        Opc_Invalid
    };



    struct DecodeResult
    {
        OpcodeId oid;
        const InstructionInfo* info;
        uint32_t opcode;
        uint64_t varArg;
    };

    struct Symbol
    {
        enum Type { Code, Data, Comment };
        Type type{ Data };
        std::string name;
        uint32_t value{};
    };

    SaturnDecoder() = default;
    virtual ~SaturnDecoder() = default;
    virtual unsigned readNibble(const uint32_t address) const = 0;
    uint64_t readNibbles(int n, uint32_t& address) const
    {
        uint64_t result = 0;
        for (uint32_t i = 0; i < n; ++i) {
            result |= readNibble(address++) << (i*4);
        }
        return result;
    }
    template <int N>
    uint64_t readNibbles(uint32_t& address) const
    {
        uint64_t result = 0;
        for (uint32_t i = 0; i < N; ++i) {
            result |= readNibble(address++) << (i*4);
        }
        return result;
    }
    DecodeResult decode(uint32_t& address) const;
    template<typename T>
    static T getParameter(const DecodeResult& decoded, uint32_t startAddress, unsigned index);
    const std::array<const std::array<std::string_view,16>,11>& getVarTables() const;
    static int64_t twosComplement(const uint64_t value, const unsigned bitSize)
    {
        if (value & (1ULL << (bitSize - 1))) {
            return static_cast<int64_t>(value) - (1LL << bitSize);
        }
        return static_cast<int64_t>(value);
    }
    static uint64_t reverseNibbles(uint64_t value, int n) {
        uint64_t result = 0;
        for (int i = 0; i < n; i++) {
            uint8_t nibble = (value >> (i * 4)) & 0xF;
            result |= (uint64_t)nibble << ((n - 1 - i) * 4);
        }
        return result;
    }
    static size_t numInstructions();
    static const InstructionInfo* getInstructionInfo(size_t index);
protected:
    //static constexpr std::array<InstructionInfo, 189> instructions;
};

}
