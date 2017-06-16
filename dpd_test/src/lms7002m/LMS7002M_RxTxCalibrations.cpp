#include "LMS7002M.h"
#include "ErrorReporting.h"
#include <assert.h>
#include "MCU_BD.h"
#include "IConnection.h"
#include "mcu_programs.h"
#include "LMS64CProtocol.h"
#include <vector>
#include <ciso646>
#define LMS_VERBOSE_OUTPUT

///define for parameter enumeration if prefix might be needed
#define LMS7param(id) id

using namespace lime;

float_type calibUserBwDivider = 5;
const static uint16_t MCU_PARAMETER_ADDRESS = 0x002D; //register used to pass parameter values to MCU
#define MCU_ID_DC_IQ_CALIBRATIONS 0x01
#define MCU_FUNCTION_CALIBRATE_TX 1
#define MCU_FUNCTION_CALIBRATE_RX 2
#define MCU_FUNCTION_READ_RSSI 3
#define MCU_FUNCTION_UPDATE_REF_CLK 4

#ifdef ENABLE_CALIBRATION_USING_FFT
    #include "kiss_fft.h"
    int fftBin = 0; //which bin to use when calibration using FFT
    bool rssiFromFFT = false;
#endif // ENABLE_CALIBRATION_USING_FFT

const float calibrationSXOffset_Hz = 4e6;

const int16_t firCoefs[] =
{
    8,
    4,
    0,
    -6,
    -11,
    -16,
    -20,
    -22,
    -22,
    -20,
    -14,
    -5,
    6,
    20,
    34,
    46,
    56,
    61,
    58,
    48,
    29,
    3,
    -29,
    -63,
    -96,
    -123,
    -140,
    -142,
    -128,
    -94,
    -44,
    20,
    93,
    167,
    232,
    280,
    302,
    291,
    244,
    159,
    41,
    -102,
    -258,
    -409,
    -539,
    -628,
    -658,
    -614,
    -486,
    -269,
    34,
    413,
    852,
    1328,
    1814,
    2280,
    2697,
    3038,
    3277,
    3401,
    3401,
    3277,
    3038,
    2697,
    2280,
    1814,
    1328,
    852,
    413,
    34,
    -269,
    -486,
    -614,
    -658,
    -628,
    -539,
    -409,
    -258,
    -102,
    41,
    159,
    244,
    291,
    302,
    280,
    232,
    167,
    93,
    20,
    -44,
    -94,
    -128,
    -142,
    -140,
    -123,
    -96,
    -63,
    -29,
    3,
    29,
    48,
    58,
    61,
    56,
    46,
    34,
    20,
    6,
    -5,
    -14,
    -20,
    -22,
    -22,
    -20,
    -16,
    -11,
    -6,
    0,
    4,
    8
};

const uint16_t backupAddrs[] = {
0x0020, 0x0082, 0x0084, 0x0085, 0x0086, 0x0087, 0x0088,
0x0089, 0x008A, 0x008B, 0x008C, 0x0100, 0x0101, 0x0102, 0x0103,
0x0104, 0x0105, 0x0106, 0x0107, 0x0108, 0x0109, 0x010A, 0x010C,
0x010D, 0x010E, 0x010F, 0x0110, 0x0111, 0x0112, 0x0113, 0x0114,
0x0115, 0x0116, 0x0117, 0x0118, 0x0119, 0x011A, 0x0200, 0x0201,
0x0202, 0x0203, 0x0204, 0x0205, 0x0206, 0x0207, 0x0208, 0x0240,
0x0241, 0x0242, 0x0243, 0x0244, 0x0245, 0x0246, 0x0247, 0x0248,
0x0249, 0x024A, 0x024B, 0x024C, 0x024D, 0x024E, 0x024F, 0x0250,
0x0251, 0x0252, 0x0253, 0x0254, 0x0255, 0x0256, 0x0257, 0x0258,
0x0259, 0x025A, 0x025B, 0x025C, 0x025D, 0x025E, 0x025F, 0x0260,
0x0261, 0x0400, 0x0401, 0x0402, 0x0403, 0x0404, 0x0405, 0x0406,
0x0407, 0x0408, 0x0409, 0x040A, 0x040C, 0x040D, 0x0440, 0x0441,
0x0442, 0x0443, 0x0444, 0x0445, 0x0446, 0x0447, 0x0448, 0x0449,
0x044A, 0x044B, 0x044C, 0x044D, 0x044E, 0x044F, 0x0450, 0x0451,
0x0452, 0x0453, 0x0454, 0x0455, 0x0456, 0x0457, 0x0458, 0x0459,
0x045A, 0x045B, 0x045C, 0x045D, 0x045E, 0x045F, 0x0460, 0x0461
};
uint16_t backupRegs[sizeof(backupAddrs) / sizeof(int16_t)];
const uint16_t backupSXAddr[] = { 0x011C, 0x011D, 0x011E, 0x011F, 0x0120, 0x0121, 0x0122, 0x0123, 0x0124 };
uint16_t backupRegsSXR[sizeof(backupSXAddr) / sizeof(int16_t)];
uint16_t backupRegsSXT[sizeof(backupSXAddr) / sizeof(int16_t)];
int16_t rxGFIR3_backup[sizeof(firCoefs) / sizeof(int16_t)];
uint16_t backup0x010D;
uint16_t backup0x0100;

inline uint16_t pow2(const uint8_t power)
{
    assert(power >= 0 && power < 16);
    return 1 << power;
}

bool sign(const int number)
{
    return number < 0;
}

/** @brief Parameters setup instructions for Tx calibration
    @return 0-success, other-failure
*/
int LMS7002M::CalibrateTxSetup(float_type bandwidth_Hz, const bool useExtLoopback)
{
    //Stage 2
    uint8_t ch = (uint8_t)Get_SPI_Reg_bits(LMS7param(MAC));
    uint8_t sel_band1_trf = (uint8_t)Get_SPI_Reg_bits(LMS7param(SEL_BAND1_TRF));
    uint8_t sel_band2_trf = (uint8_t)Get_SPI_Reg_bits(LMS7param(SEL_BAND2_TRF));

    //rfe
    //reset RFE to defaults
    SetDefaults(RFE);
    if(useExtLoopback)
    {
        int LNAselection = 1;
        Modify_SPI_Reg_bits(LMS7param(SEL_PATH_RFE), LNAselection); //SEL_PATH_RFE 3
        Modify_SPI_Reg_bits(LMS7param(G_LNA_RFE), 1);
        Modify_SPI_Reg_bits(0x010C, 4, 3, 0); //PD_MXLOBUF_RFE 0, PD_QGEN_RFE 0
        Modify_SPI_Reg_bits(LMS7param(CCOMP_TIA_RFE), 4);
        Modify_SPI_Reg_bits(LMS7param(CFB_TIA_RFE), 50);
        Modify_SPI_Reg_bits(LMS7param(ICT_LODC_RFE), 31); //ICT_LODC_RFE 31
        if(LNAselection == 2)
        {
            Modify_SPI_Reg_bits(LMS7param(EN_INSHSW_L_RFE), 0);
            Modify_SPI_Reg_bits(LMS7param(EN_INSHSW_W_RFE), 1);
        }
        else if(LNAselection == 3)
        {
            Modify_SPI_Reg_bits(LMS7param(EN_INSHSW_L_RFE), 1);
            Modify_SPI_Reg_bits(LMS7param(EN_INSHSW_W_RFE), 0);
        }
        else
        {
            Modify_SPI_Reg_bits(LMS7param(EN_INSHSW_L_RFE), 1);
            Modify_SPI_Reg_bits(LMS7param(EN_INSHSW_W_RFE), 1);
        }
        Modify_SPI_Reg_bits(LMS7param(PD_LNA_RFE), 0);
        Modify_SPI_Reg_bits(LMS7param(EN_DCOFF_RXFE_RFE), 1);
        Modify_SPI_Reg_bits(LMS7param(G_TIA_RFE), 1);
    }
    else
    {
        if(sel_band1_trf == 1)
            Modify_SPI_Reg_bits(LMS7param(SEL_PATH_RFE), 3); //SEL_PATH_RFE 3
        else if(sel_band2_trf == 1)
            Modify_SPI_Reg_bits(LMS7param(SEL_PATH_RFE), 2);
        else
            return ReportError(EINVAL, "Tx Calibration: band not selected");

        Modify_SPI_Reg_bits(LMS7param(G_RXLOOPB_RFE), 7);
        Modify_SPI_Reg_bits(0x010C, 4, 3, 0); //PD_MXLOBUF_RFE 0, PD_QGEN_RFE 0
        Modify_SPI_Reg_bits(LMS7param(CCOMP_TIA_RFE), 4);
        Modify_SPI_Reg_bits(LMS7param(CFB_TIA_RFE), 50);
        Modify_SPI_Reg_bits(LMS7param(ICT_LODC_RFE), 31); //ICT_LODC_RFE 31
        if(sel_band1_trf)
        {
            Modify_SPI_Reg_bits(LMS7param(PD_RLOOPB_1_RFE), 0);
            Modify_SPI_Reg_bits(LMS7param(PD_RLOOPB_2_RFE), 1);
            Modify_SPI_Reg_bits(LMS7param(EN_INSHSW_LB1_RFE), 0);
            Modify_SPI_Reg_bits(LMS7param(EN_INSHSW_LB2_RFE), 1);
        }
        else
        {
            Modify_SPI_Reg_bits(LMS7param(PD_RLOOPB_1_RFE), 1);
            Modify_SPI_Reg_bits(LMS7param(PD_RLOOPB_2_RFE), 0);
            Modify_SPI_Reg_bits(LMS7param(EN_INSHSW_LB1_RFE), 1);
            Modify_SPI_Reg_bits(LMS7param(EN_INSHSW_LB2_RFE), 0);
        }
        Modify_SPI_Reg_bits(LMS7param(EN_DCOFF_RXFE_RFE), 1);
    }

    //RBB
    //reset RBB to defaults
    SetDefaults(RBB);
    Modify_SPI_Reg_bits(LMS7param(PD_LPFL_RBB), 1);
    Modify_SPI_Reg_bits(LMS7param(G_PGA_RBB), 0);
    Modify_SPI_Reg_bits(LMS7param(INPUT_CTL_PGA_RBB), 2);
    Modify_SPI_Reg_bits(LMS7param(ICT_PGA_OUT_RBB), 12);
    Modify_SPI_Reg_bits(LMS7param(ICT_PGA_IN_RBB), 12);

    //TRF
    Modify_SPI_Reg_bits(LMS7param(L_LOOPB_TXPAD_TRF), 0); //L_LOOPB_TXPAD_TRF 0
    if(useExtLoopback)
    {
        Modify_SPI_Reg_bits(LMS7param(EN_LOOPB_TXPAD_TRF), 0); //EN_LOOPB_TXPAD_TRF 1
        Modify_SPI_Reg_bits(LMS7param(EN_G_TRF), 1);
    }
    else
        Modify_SPI_Reg_bits(LMS7param(EN_LOOPB_TXPAD_TRF), 1); //EN_LOOPB_TXPAD_TRF 1

    //AFE
    Modify_SPI_Reg_bits(LMS7param(PD_RX_AFE2), 0); //PD_RX_AFE2 0

    //BIAS
    uint16_t backup = Get_SPI_Reg_bits(LMS7param(RP_CALIB_BIAS));
    SetDefaults(BIAS);
    Modify_SPI_Reg_bits(LMS7param(RP_CALIB_BIAS), backup); //RP_CALIB_BIAS

    //XBUF
    Modify_SPI_Reg_bits(0x0085, 2, 0, 1); //PD_XBUF_RX 0, PD_XBUF_TX 0, EN_G_XBUF 1

    //CGEN
    //reset CGEN to defaults
    const float_type cgenFreq = GetFrequencyCGEN();
    SetDefaults(CGEN);
    int cgenMultiplier = int((cgenFreq / 46.08e6) + 0.5);
    if(cgenMultiplier < 2)
        cgenMultiplier = 2;
    if(cgenMultiplier > 13)
        cgenMultiplier = 13;
    //power up VCO
    Modify_SPI_Reg_bits(0x0086, 2, 2, 0);

    int status = SetFrequencyCGEN(46.08e6 * cgenMultiplier);
    if(status != 0)
        return status;

    //SXR
    Modify_SPI_Reg_bits(LMS7param(MAC), 1);
    SetDefaults(SX);
    Modify_SPI_Reg_bits(LMS7param(PD_VCO), 0);
    Modify_SPI_Reg_bits(LMS7param(ICT_VCO), 200);
    {
        float_type SXTfreq = GetFrequencySX(Tx);
        float_type SXRfreq = SXTfreq - bandwidth_Hz / calibUserBwDivider - calibrationSXOffset_Hz;
        status = SetFrequencySX(Rx, SXRfreq);
        if(status != 0)
            return status;
        status = TuneVCO(VCO_SXR);
        if(status != 0)
            return status;
    }

    //SXT
    Modify_SPI_Reg_bits(LMS7param(MAC), 2);
    Modify_SPI_Reg_bits(PD_LOCH_T2RBUF, 1);
    Modify_SPI_Reg_bits(LMS7param(MAC), ch);

    //TXTSP
    //check if user uses GFIR
    bool GFIR_active[3] = { false, false, false };
    uint8_t gfir_byps[3];
    uint8_t gfir_l[3];
    uint8_t gfir_n[3];
    const uint8_t coefsToCheck = 5;
    int16_t txGFIR_coefs[coefsToCheck];
    gfir_byps[0] = Get_SPI_Reg_bits(GFIR1_BYP_TXTSP);
    gfir_byps[1] = Get_SPI_Reg_bits(GFIR2_BYP_TXTSP);
    gfir_byps[2] = Get_SPI_Reg_bits(GFIR3_BYP_TXTSP);

    if(gfir_byps[0] == 0)
    {
        GetGFIRCoefficients(LMS7002M::Tx, 0, txGFIR_coefs, coefsToCheck);
        for(int i = 0; i < coefsToCheck; ++i)
            if(txGFIR_coefs[i] != 0)
            {
                GFIR_active[0] = true;
                gfir_l[0] = Get_SPI_Reg_bits(GFIR1_L_TXTSP);
                gfir_n[0] = Get_SPI_Reg_bits(GFIR1_N_TXTSP);
                break;
            }
    }
    if(gfir_byps[1] == 0)
    {
        GetGFIRCoefficients(LMS7002M::Tx, 1, txGFIR_coefs, coefsToCheck);
        for(int i = 0; i < coefsToCheck; ++i)
            if(txGFIR_coefs[i] != 0)
            {
                GFIR_active[1] = true;
                gfir_l[1] = Get_SPI_Reg_bits(GFIR2_L_TXTSP);
                gfir_n[1] = Get_SPI_Reg_bits(GFIR2_N_TXTSP);
                break;
            }
    }
    if(gfir_byps[2] == 0)
    {
        GetGFIRCoefficients(LMS7002M::Tx, 2, txGFIR_coefs, coefsToCheck);
        for(int i = 0; i < coefsToCheck; ++i)
            if(txGFIR_coefs[i] != 0)
            {
                GFIR_active[2] = true;
                gfir_l[2] = Get_SPI_Reg_bits(GFIR3_L_TXTSP);
                gfir_n[2] = Get_SPI_Reg_bits(GFIR3_N_TXTSP);
                break;
            }
    }
    SetDefaults(TxTSP); //GFIR coefficients are not reset
    SetDefaults(TxNCO);
    if(GFIR_active[0])
    {
        Modify_SPI_Reg_bits(GFIR1_BYP_TXTSP, gfir_byps[0]);
        Modify_SPI_Reg_bits(GFIR1_L_TXTSP, gfir_l[0]);
        Modify_SPI_Reg_bits(GFIR1_N_TXTSP, gfir_n[0]);
    }
    if(GFIR_active[1])
    {
        Modify_SPI_Reg_bits(GFIR2_BYP_TXTSP, gfir_byps[1]);
        Modify_SPI_Reg_bits(GFIR2_L_TXTSP, gfir_l[1]);
        Modify_SPI_Reg_bits(GFIR2_N_TXTSP, gfir_n[1]);
    }
    if(GFIR_active[2])
    {
        Modify_SPI_Reg_bits(GFIR3_BYP_TXTSP, gfir_byps[2]);
        Modify_SPI_Reg_bits(GFIR3_L_TXTSP, gfir_l[2]);
        Modify_SPI_Reg_bits(GFIR3_N_TXTSP, gfir_n[2]);
    }
    Modify_SPI_Reg_bits(LMS7param(TSGMODE_TXTSP), 1);
    Modify_SPI_Reg_bits(LMS7param(INSEL_TXTSP), 1);
    if(!GFIR_active[0] && !GFIR_active[1] && !GFIR_active[2])
        Modify_SPI_Reg_bits(0x0208, 6, 4, 0x7); //GFIR3_BYP 1, GFIR2_BYP 1, GFIR1_BYP 1
    if(useExtLoopback)
        Modify_SPI_Reg_bits(LMS7param(CMIX_BYP_TXTSP), 1);
    LoadDC_REG_IQ(Tx, (int16_t)0x7FFF, (int16_t)0x8000);
    SetNCOFrequency(Tx, 0, bandwidth_Hz / calibUserBwDivider);

    //RXTSP
    SetDefaults(RxTSP);
    SetDefaults(RxNCO);
    Modify_SPI_Reg_bits(LMS7param(GFIR2_BYP_RXTSP), 1);
    Modify_SPI_Reg_bits(LMS7param(GFIR1_BYP_RXTSP), 1);
    Modify_SPI_Reg_bits(LMS7param(HBD_OVR_RXTSP), 4); //Decimation HBD ratio

    if(useExtLoopback)
    {
        Modify_SPI_Reg_bits(LMS7param(GFIR3_BYP_RXTSP), 1);
        Modify_SPI_Reg_bits(LMS7param(AGC_BYP_RXTSP), 1);
    }
    else
    {
        Modify_SPI_Reg_bits(LMS7param(AGC_MODE_RXTSP), 1);
        Modify_SPI_Reg_bits(LMS7param(CMIX_BYP_RXTSP), 1);
        Modify_SPI_Reg_bits(LMS7param(CMIX_GAIN_RXTSP), 1);
        Modify_SPI_Reg_bits(LMS7param(AGC_AVG_RXTSP), 0x1);
        Modify_SPI_Reg_bits(LMS7param(GFIR3_L_RXTSP), 7);
        Modify_SPI_Reg_bits(LMS7param(GFIR3_N_RXTSP), 4 * cgenMultiplier - 1);

        SetGFIRCoefficients(Rx, 2, firCoefs, sizeof(firCoefs) / sizeof(int16_t));
    }
    if(ch == 2)
    {
        Modify_SPI_Reg_bits(MAC, 1);
        Modify_SPI_Reg_bits(LMS7param(PD_TX_AFE2), 0);
        Modify_SPI_Reg_bits(LMS7param(EN_NEXTRX_RFE), 1); // EN_NEXTTX_RFE 1
        Modify_SPI_Reg_bits(LMS7param(EN_NEXTTX_TRF), 1); //EN_NEXTTX_TRF 1
        Modify_SPI_Reg_bits(MAC, ch);
    }
    return 0;
}

/** @brief Flips the CAPTURE bit and returns digital RSSI value
*/
uint32_t LMS7002M::GetRSSI()
{
#ifdef ENABLE_CALIBRATION_USING_FFT
    if(!rssiFromFFT)
    {
        const int fftSize = 16384;

        StreamConfig config;
        config.isTx = false;
        config.channels.push_back(0);
        config.channels.push_back(1);
        config.format = StreamConfig::STREAM_12_BIT_IN_16;
        config.bufferLength = fftSize;
        size_t streamID(~0);
        controlPort->SetHardwareTimestamp(0);
        const auto errorMsg = controlPort->SetupStream(streamID, config);

        float avgRSSI = 0;
        const int channelsCount = 1;

        kiss_fft_cfg m_fftCalcPlan = kiss_fft_alloc(fftSize, 0, 0, 0);
        kiss_fft_cpx* m_fftCalcIn = new kiss_fft_cpx[fftSize];
        kiss_fft_cpx* m_fftCalcOut = new kiss_fft_cpx[fftSize];

        int16_t **buffs = new int16_t*[channelsCount];
        for(int i=0; i<channelsCount; ++i)
            buffs[i] = new int16_t[fftSize];

        //TODO setup streaming

        StreamMetadata metadata;
        auto ret = controlPort->ReadStream(streamID, (void* const*)buffs, fftSize, 1000, metadata);

        long samplesCollected = 0;
        int16_t sample = 0;

        const int stepSize = 4;
        /*for (uint16_t b = 0; b < bytesReceived; b += stepSize)
        {
            //I sample
            sample = (buffer[b] & 0xFF);
            sample |= (buffer[b + 1] & 0x0F) << 8;

            sample = sample << 4;
            sample = sample >> 4;
            m_fftCalcIn[samplesCollected].r = sample;

            //Q sample
            sample = (buffer[b + 2] & 0xFF);
            sample |= (buffer[b + 3] & 0x0F) << 8;

            sample = sample << 4;
            sample = sample >> 4;
            m_fftCalcIn[samplesCollected].i = sample;
            ++samplesCollected;
            if (samplesCollected >= fftSize)
                break;
        }*/

        kiss_fft(m_fftCalcPlan, m_fftCalcIn, m_fftCalcOut);
        for (int i = 0; i < fftSize; ++i)
        {
            // normalize FFT results
            m_fftCalcOut[i].r /= fftSize;
            m_fftCalcOut[i].i /= fftSize;
        }

        std::vector<float> fftBins_dbFS;
        fftBins_dbFS.resize(fftSize, 0);
        int output_index = 0;

        for (int i = 0; i < fftSize; ++i)
        {
            fftBins_dbFS[output_index++] = sqrt(m_fftCalcOut[i].r * m_fftCalcOut[i].r + m_fftCalcOut[i].i * m_fftCalcOut[i].i);
        }

        for (int s = 0; s < fftSize; ++s)
            fftBins_dbFS[s] = (fftBins_dbFS[s] != 0 ? (20 * log10(fftBins_dbFS[s])) - 69.2369 : -300);

        int binToGet = fftBin;
        float rssiToReturn = m_fftCalcOut[binToGet].r * m_fftCalcOut[binToGet].r + m_fftCalcOut[binToGet].i * m_fftCalcOut[binToGet].i;
        avgRSSI = fftBins_dbFS[binToGet];
        kiss_fft_free(m_fftCalcPlan);
        for(int i=0; i<channelsCount; ++i)
            delete buffs[i];
        delete[]buffs;

        printf("FFT RSSI = %f \n", avgRSSI);
        controlPort->CloseStream(streamID);
        return avgRSSI;
    }
#endif
    Modify_SPI_Reg_bits(LMS7param(CAPTURE), 0);
    Modify_SPI_Reg_bits(LMS7param(CAPTURE), 1);
    return (Get_SPI_Reg_bits(0x040F, 15, 0, true) << 2) | Get_SPI_Reg_bits(0x040E, 1, 0, true);
}

/** @brief Calibrates Transmitter. DC correction, IQ gains, IQ phase correction
@return 0-success, other-failure
*/
int LMS7002M::CalibrateTx(float_type bandwidth_Hz, const bool useExtLoopback)
{
    if(useExtLoopback)
        return ReportError(EPERM, "Calibration with external loopback not yet implemented");
    int status;
    if(mCalibrationByMCU)
    {
        uint8_t mcuID = mcuControl->ReadMCUProgramID();
        if(mcuID != MCU_ID_DC_IQ_CALIBRATIONS)
        {
            status = mcuControl->Program_MCU(mcu_program_lms7_dc_iq_calibration_bin, MCU_BD::SRAM);
            if(status != 0)
                return status;
        }
    }

    Channel ch = this->GetActiveChannel();
    uint8_t sel_band1_trf = (uint8_t)Get_SPI_Reg_bits(LMS7param(SEL_BAND1_TRF));
    uint8_t sel_band2_trf = (uint8_t)Get_SPI_Reg_bits(LMS7param(SEL_BAND2_TRF));

    uint32_t boardId = controlPort->GetDeviceInfo().boardSerialNumber;
    double txFreq = GetFrequencySX(Tx);
    uint8_t channel = ch == 1 ? 0 : 1;
    bool foundInCache = false;
    int band = sel_band1_trf ? 0 : 1;

    uint16_t gainAddr;
    uint16_t gcorri;
    uint16_t gcorrq;
    uint16_t dccorri;
    uint16_t dccorrq;
    int16_t phaseOffset;
    int16_t gain = 1983;
    const uint16_t gainMSB = 10;
    const uint16_t gainLSB = 0;

    if(useCache)
    {
        int dcI, dcQ, gainI, gainQ, phOffset;

        foundInCache = (valueCache.GetDC_IQ(boardId, txFreq*1e6, channel, true, band, &dcI, &dcQ, &gainI, &gainQ, &phOffset) == 0);
        if(foundInCache)
        {
            printf("Tx calibration: using cached values\n");
            dccorri = dcI;
            dccorrq = dcQ;
            gcorri = gainI;
            gcorrq = gainQ;
            phaseOffset = phOffset;
            Modify_SPI_Reg_bits(LMS7param(DCCORRI_TXTSP), dcI);
            Modify_SPI_Reg_bits(LMS7param(DCCORRQ_TXTSP), dcQ);
            Modify_SPI_Reg_bits(LMS7param(GCORRI_TXTSP), gainI);
            Modify_SPI_Reg_bits(LMS7param(GCORRQ_TXTSP), gainQ);
            Modify_SPI_Reg_bits(LMS7param(IQCORR_TXTSP), phaseOffset);
            Modify_SPI_Reg_bits(LMS7param(DC_BYP_TXTSP), 0); //DC_BYP
            Modify_SPI_Reg_bits(0x0208, 1, 0, 0); //GC_BYP PH_BYP
            return 0;
        }
    }

    LMS7002M_SelfCalState state(this);

    Log("Tx calibration started", LOG_INFO);
    BackupAllRegisters();

    Log("Setup stage", LOG_INFO);
    status = CalibrateTxSetup(bandwidth_Hz, useExtLoopback);
    if(status != 0)
        goto TxCalibrationEnd; //go to ending stage to restore registers
    if(mCalibrationByMCU)
    {
        //set reference clock parameter inside MCU
        long refClk = GetReferenceClk_SX(false);
        uint16_t refClkToMCU = (int(refClk / 1000000) << 9) | ((refClk % 1000000) / 10000);
        SPI_write(MCU_PARAMETER_ADDRESS, refClkToMCU);
        mcuControl->CallMCU(MCU_FUNCTION_UPDATE_REF_CLK);
        auto statusMcu = mcuControl->WaitForMCU(100);
        //set bandwidth for MCU to read from register, value is integer stored in MHz
        SPI_write(MCU_PARAMETER_ADDRESS, (uint16_t)(bandwidth_Hz / 1e6));
        mcuControl->CallMCU(MCU_FUNCTION_CALIBRATE_TX);
        statusMcu = mcuControl->WaitForMCU(30000);
        if(statusMcu == 0)
        {
            printf("MCU working too long %i\n", statusMcu);
        }
    }
    else
    {
        CheckSaturationTxRx(bandwidth_Hz, useExtLoopback);

        Modify_SPI_Reg_bits(EN_G_TRF, 0);
        if(!useExtLoopback)
            CalibrateRxDC_RSSI();
        CalibrateTxDC_RSSI(bandwidth_Hz);

        //TXIQ
        Modify_SPI_Reg_bits(EN_G_TRF, 1);
        Modify_SPI_Reg_bits(CMIX_BYP_TXTSP, 0);

        SetNCOFrequency(LMS7002M::Rx, 0, calibrationSXOffset_Hz - 0.1e6);

        //coarse gain
        uint32_t rssiIgain;
        uint32_t rssiQgain;
        Modify_SPI_Reg_bits(GCORRI_TXTSP, 2047 - 64);
        Modify_SPI_Reg_bits(GCORRQ_TXTSP, 2047);
        rssiIgain = GetRSSI();
        Modify_SPI_Reg_bits(GCORRI_TXTSP, 2047);
        Modify_SPI_Reg_bits(GCORRQ_TXTSP, 2047 - 64);
        rssiQgain = GetRSSI();

        Modify_SPI_Reg_bits(GCORRI_TXTSP, 2047);
        Modify_SPI_Reg_bits(GCORRQ_TXTSP, 2047);

        if(rssiIgain < rssiQgain)
            gainAddr = GCORRI_TXTSP.address;
        else
            gainAddr = GCORRQ_TXTSP.address;

        CoarseSearch(gainAddr, gainMSB, gainLSB, gain, 7);
#ifdef LMS_VERBOSE_OUTPUT
        printf("Coarse search Tx GAIN_%s: %i\n", gainAddr == GCORRI_TXTSP.address ? "I" : "Q", gain);
#endif
        //coarse phase offset
        uint32_t rssiUp;
        uint32_t rssiDown;
        Modify_SPI_Reg_bits(IQCORR_TXTSP, 15);
        rssiUp = GetRSSI();
        Modify_SPI_Reg_bits(IQCORR_TXTSP, -15);
        rssiDown = GetRSSI();
        if(rssiUp > rssiDown)
            phaseOffset = -64;
        else if(rssiUp < rssiDown)
            phaseOffset = 192;
        else
            phaseOffset = 64;

        Modify_SPI_Reg_bits(IQCORR_TXTSP, phaseOffset);
        CoarseSearch(IQCORR_TXTSP.address, IQCORR_TXTSP.msb, IQCORR_TXTSP.lsb, phaseOffset, 7);
#ifdef LMS_VERBOSE_OUTPUT
        printf("Coarse search Tx IQCORR: %i\n", phaseOffset);
#endif
        CoarseSearch(gainAddr, gainMSB, gainLSB, gain, 4);
#ifdef LMS_VERBOSE_OUTPUT
        printf("Coarse search Tx GAIN_%s: %i\n", gainAddr == GCORRI_TXTSP.address ? "I" : "Q", gain);
        printf("Fine search Tx GAIN_%s/IQCORR...\n", gainAddr == GCORRI_TXTSP.address ? "I" : "Q");
#endif
        FineSearch(gainAddr, gainMSB, gainLSB, gain, IQCORR_TXTSP.address, IQCORR_TXTSP.msb, IQCORR_TXTSP.lsb, phaseOffset, 7);
#ifdef LMS_VERBOSE_OUTPUT
        printf("Fine search Tx GAIN_%s: %i, IQCORR: %i\n", gainAddr == GCORRI_TXTSP.address ? "I" : "Q", gain, phaseOffset);
#endif
        Modify_SPI_Reg_bits(gainAddr, gainMSB, gainLSB, gain);
        Modify_SPI_Reg_bits(IQCORR_TXTSP.address, IQCORR_TXTSP.msb, IQCORR_TXTSP.lsb, phaseOffset);
    }
    dccorri = Get_SPI_Reg_bits(LMS7param(DCCORRI_TXTSP), true);
    dccorrq = Get_SPI_Reg_bits(LMS7param(DCCORRQ_TXTSP), true);
    gcorri = Get_SPI_Reg_bits(LMS7param(GCORRI_TXTSP), true);
    gcorrq = Get_SPI_Reg_bits(LMS7param(GCORRQ_TXTSP), true);
    phaseOffset = Get_SPI_Reg_bits(LMS7param(IQCORR_TXTSP), true);
TxCalibrationEnd:
    Log("Restoring registers state", LOG_INFO);
    Modify_SPI_Reg_bits(LMS7param(MAC), ch);
    RestoreAllRegisters();

    if(status != 0)
    {
        Log("Tx calibration failed", LOG_WARNING);
        return status;
    }

    if(useCache)
        valueCache.InsertDC_IQ(boardId, txFreq*1e6, channel, true, band, dccorri, dccorrq, gcorri, gcorrq, phaseOffset);

    Modify_SPI_Reg_bits(LMS7param(MAC), ch);
    Modify_SPI_Reg_bits(LMS7param(DCCORRI_TXTSP), dccorri);
    Modify_SPI_Reg_bits(LMS7param(DCCORRQ_TXTSP), dccorrq);
    Modify_SPI_Reg_bits(LMS7param(GCORRI_TXTSP), gcorri);
    Modify_SPI_Reg_bits(LMS7param(GCORRQ_TXTSP), gcorrq);
    Modify_SPI_Reg_bits(LMS7param(IQCORR_TXTSP), phaseOffset);

    Modify_SPI_Reg_bits(LMS7param(DC_BYP_TXTSP), 0); //DC_BYP
    Modify_SPI_Reg_bits(0x0208, 1, 0, 0); //GC_BYP PH_BYP
    LoadDC_REG_IQ(Tx, (int16_t)0x7FFF, (int16_t)0x8000);
    Log("Tx calibration finished", LOG_INFO);
    return 0;
}

/** @brief Performs Rx DC offsets calibration
*/
void LMS7002M::CalibrateRxDC_RSSI()
{
#ifdef ENABLE_CALIBRATION_USING_FFT
    fftBin = 0;
#endif
    int16_t offsetI = 32;
    int16_t offsetQ = 32;
    Modify_SPI_Reg_bits(DC_BYP_RXTSP, 1);
    Modify_SPI_Reg_bits(CAPSEL, 0);
    SetRxDCOFF(offsetI, offsetQ);
    //find I
    CoarseSearch(DCOFFI_RFE.address, DCOFFI_RFE.msb, DCOFFI_RFE.lsb, offsetI, 6);

    //find Q
    CoarseSearch(DCOFFQ_RFE.address, DCOFFQ_RFE.msb, DCOFFQ_RFE.lsb, offsetQ, 6);

    CoarseSearch(DCOFFI_RFE.address, DCOFFI_RFE.msb, DCOFFI_RFE.lsb, offsetI, 3);
    CoarseSearch(DCOFFQ_RFE.address, DCOFFQ_RFE.msb, DCOFFQ_RFE.lsb, offsetQ, 3);
#ifdef LMS_VERBOSE_OUTPUT
    printf("Fine search Rx DCOFFI/DCOFFQ\n");
#endif
    FineSearch(DCOFFI_RFE.address, DCOFFI_RFE.msb, DCOFFI_RFE.lsb, offsetI, DCOFFQ_RFE.address, DCOFFQ_RFE.msb, DCOFFQ_RFE.lsb, offsetQ, 5);
#ifdef LMS_VERBOSE_OUTPUT
    printf("Fine search Rx DCOFFI: %i, DCOFFQ: %i\n", offsetI, offsetQ);
#endif
    SetRxDCOFF(offsetI, offsetQ);
    Modify_SPI_Reg_bits(DC_BYP_RXTSP, 0); // DC_BYP 0
#ifdef ENABLE_CALIBRATION_USING_FFT
    fftBin = 569; //fft bin 100 kHz
#endif
}

/** @brief Parameters setup instructions for Rx calibration
@param bandwidth_Hz filter bandwidth in Hz
@return 0-success, other-failure
*/
int LMS7002M::CalibrateRxSetup(float_type bandwidth_Hz, const bool useExtLoopback)
{
    uint8_t ch = (uint8_t)Get_SPI_Reg_bits(LMS7param(MAC));

    //rfe
    Modify_SPI_Reg_bits(LMS7param(EN_DCOFF_RXFE_RFE), 1);
    if(not useExtLoopback)
        Modify_SPI_Reg_bits(LMS7param(G_RXLOOPB_RFE), 3);
    Modify_SPI_Reg_bits(0x010C, 4, 3, 0); //PD_MXLOBUF_RFE 0, PD_QGEN_RFE 0
    Modify_SPI_Reg_bits(0x010C, 1, 1, 0); //PD_TIA 0
    Modify_SPI_Reg_bits(0x0110, 4, 0, 31); //ICT_LO_RFE 31

    //RBB
    Modify_SPI_Reg_bits(0x0115, 15, 14, 0); //Loopback switches disable
    Modify_SPI_Reg_bits(0x0119, 15, 15, 0); //OSW_PGA 0

    //TRF
    //reset TRF to defaults
    SetDefaults(TRF);
    if(not useExtLoopback)
    {
        Modify_SPI_Reg_bits(L_LOOPB_TXPAD_TRF, 0);
        Modify_SPI_Reg_bits(EN_LOOPB_TXPAD_TRF, 1);
    }
    else
        Modify_SPI_Reg_bits(LOSS_MAIN_TXPAD_TRF, 10);
    Modify_SPI_Reg_bits(EN_G_TRF, 0);

    {
        uint8_t selPath;
        if(useExtLoopback) //use PA1
            selPath = 3;
        else
            selPath = Get_SPI_Reg_bits(SEL_PATH_RFE);

        if (selPath == 2)
        {
            Modify_SPI_Reg_bits(SEL_BAND2_TRF, 1);
            Modify_SPI_Reg_bits(SEL_BAND1_TRF, 0);
        }
        else if (selPath == 3)
        {
            Modify_SPI_Reg_bits(SEL_BAND2_TRF, 0);
            Modify_SPI_Reg_bits(SEL_BAND1_TRF, 1);
        }
        else
            return ReportError("CalibrateRxSetup() - SEL_PATH_RFE must be LNAL or LNAW"); //todo restore settings
    }

    //TBB
    //reset TBB to defaults
    SetDefaults(TBB);
    Modify_SPI_Reg_bits(LMS7param(CG_IAMP_TBB), 1);
    Modify_SPI_Reg_bits(LMS7param(ICT_IAMP_FRP_TBB), 1); //ICT_IAMP_FRP_TBB 1
    Modify_SPI_Reg_bits(LMS7param(ICT_IAMP_GG_FRP_TBB), 6); //ICT_IAMP_GG_FRP_TBB 6

    //AFE
    Modify_SPI_Reg_bits(LMS7param(PD_RX_AFE2), 0); //PD_RX_AFE2

    //BIAS
    {
        uint16_t rp_calib_bias = Get_SPI_Reg_bits(0x0084, 10, 6);
        SetDefaults(BIAS);
        Modify_SPI_Reg_bits(0x0084, 10, 6, rp_calib_bias); //RP_CALIB_BIAS
    }

    //XBUF
    Modify_SPI_Reg_bits(0x0085, 2, 0, 1); //PD_XBUF_RX 0, PD_XBUF_TX 0, EN_G_XBUF 1

    //CGEN
    //reset CGEN to defaults
    const float_type cgenFreq = GetFrequencyCGEN();
    SetDefaults(CGEN);
    int cgenMultiplier = int(cgenFreq / 46.08e6 + 0.5);
    if(cgenMultiplier < 2)
        cgenMultiplier = 2;
    if(cgenMultiplier > 13)
        cgenMultiplier = 13;
    //power up VCO
    Modify_SPI_Reg_bits(0x0086, 2, 2, 0);

    int status = SetFrequencyCGEN(46.08e6 * cgenMultiplier);
    if(status != 0)
        return status;

    Modify_SPI_Reg_bits(MAC, 2);
    bool isTDD = Get_SPI_Reg_bits(PD_LOCH_T2RBUF, true) == 0;
    if(isTDD)
    {
        //in TDD do nothing
        Modify_SPI_Reg_bits(MAC, 1);
        SetDefaults(SX);
        SetFrequencySX(false, GetFrequencySX(true) - bandwidth_Hz / calibUserBwDivider - 9e6);
        Modify_SPI_Reg_bits(PD_VCO, 1);
    }
    else
    {
        //SXR
        Modify_SPI_Reg_bits(LMS7param(MAC), 1);
        float_type SXRfreqHz = GetFrequencySX(Rx);

        //SXT
        Modify_SPI_Reg_bits(LMS7param(MAC), 2);
        SetDefaults(SX);
        Modify_SPI_Reg_bits(LMS7param(PD_VCO), 0);

        status = SetFrequencySX(Tx, SXRfreqHz + bandwidth_Hz / calibUserBwDivider + 9e6);
        if(status != 0) return status;
    }
    Modify_SPI_Reg_bits(LMS7param(MAC), ch);

    //TXTSP
    SetDefaults(TxTSP);
    SetDefaults(TxNCO);
    Modify_SPI_Reg_bits(LMS7param(TSGFCW_TXTSP), 1);
    Modify_SPI_Reg_bits(TSGMODE_TXTSP, 0x1); //TSGMODE 1
    Modify_SPI_Reg_bits(INSEL_TXTSP, 1);
    Modify_SPI_Reg_bits(0x0208, 6, 4, 0x7); //GFIR3_BYP 1, GFIR2_BYP 1, GFIR1_BYP 1
    Modify_SPI_Reg_bits(CMIX_GAIN_TXTSP, 0);
    Modify_SPI_Reg_bits(CMIX_SC_TXTSP, 1);
    LoadDC_REG_IQ(Tx, (int16_t)0x7FFF, (int16_t)0x8000);
    SetNCOFrequency(Tx, 0, 9e6);

    //RXTSP
    SetDefaults(RxTSP);
    SetDefaults(RxNCO);
    Modify_SPI_Reg_bits(0x040C, 4, 4, 1); //
    Modify_SPI_Reg_bits(0x040C, 3, 3, 1); //
    Modify_SPI_Reg_bits(LMS7param(HBD_OVR_RXTSP), 4);
    if(not useExtLoopback)
    {
        Modify_SPI_Reg_bits(LMS7param(AGC_MODE_RXTSP), 1); //AGC_MODE 1
        Modify_SPI_Reg_bits(0x040C, 7, 7, 0x1); //CMIX_BYP 1
        Modify_SPI_Reg_bits(LMS7param(CAPSEL), 0);
        Modify_SPI_Reg_bits(LMS7param(AGC_AVG_RXTSP), 1); //agc_avg iq corr
        Modify_SPI_Reg_bits(LMS7param(CMIX_GAIN_RXTSP), 0);
        Modify_SPI_Reg_bits(LMS7param(GFIR3_L_RXTSP), 7);
        Modify_SPI_Reg_bits(LMS7param(GFIR3_N_RXTSP), 4*cgenMultiplier - 1);
        SetGFIRCoefficients(Rx, 2, firCoefs, sizeof(firCoefs) / sizeof(int16_t));
    }
    else
    {
        Modify_SPI_Reg_bits(0x040C, 5, 5, 1); // GFIR3_BYP
        Modify_SPI_Reg_bits(AGC_BYP_RXTSP, 1);
        Modify_SPI_Reg_bits(CMIX_BYP_RXTSP, 1);
    }

    SetNCOFrequency(Rx, 0, bandwidth_Hz/calibUserBwDivider - 0.1e6);

    if(useExtLoopback)
    {
        //limelight
        Modify_SPI_Reg_bits(LML1_FIDM, 1);
        Modify_SPI_Reg_bits(LML2_FIDM, 1);
        Modify_SPI_Reg_bits(LML1_MODE, 0);
        Modify_SPI_Reg_bits(LML2_MODE, 0);
    }

    //modifications when calibrating channel B
    if(ch == 2)
    {
        Modify_SPI_Reg_bits(MAC, 1);
        Modify_SPI_Reg_bits(LMS7param(EN_NEXTRX_RFE), 1);
        Modify_SPI_Reg_bits(EN_NEXTTX_TRF, 1);
        Modify_SPI_Reg_bits(LMS7param(PD_TX_AFE2), 0);
        Modify_SPI_Reg_bits(MAC, ch);
    }
    return 0;
}

/** @brief Calibrates Receiver. DC offset, IQ gains, IQ phase correction
    @return 0-success, other-failure
*/
int LMS7002M::CalibrateRx(float_type bandwidth_Hz, const bool useExtLoopback)
{
    if(useExtLoopback)
        return ReportError(EPERM, "Calibration with external loopback not yet implemented");
    int status;
    if(mCalibrationByMCU)
    {
        uint8_t mcuID = mcuControl->ReadMCUProgramID();
        if(mcuID != MCU_ID_DC_IQ_CALIBRATIONS)
        {
            status = mcuControl->Program_MCU(mcu_program_lms7_dc_iq_calibration_bin, MCU_BD::SRAM);
            if( status != 0)
                return status;
        }
    }

    Channel ch = this->GetActiveChannel();
    uint32_t boardId = controlPort->GetDeviceInfo().boardSerialNumber;
    uint8_t channel = ch == 1 ? 0 : 1;
    uint8_t sel_path_rfe = (uint8_t)Get_SPI_Reg_bits(LMS7param(SEL_PATH_RFE));
    int lna = sel_path_rfe;

    int16_t iqcorr_rx = 0;
    int16_t dcoffi;
    int16_t dcoffq;
    int16_t gain;
    uint16_t gainAddr = 0;
    const uint8_t gainMSB = 10;
    const uint8_t gainLSB = 0;
    uint16_t mingcorri;
    uint16_t mingcorrq;
    int16_t phaseOffset;

    double rxFreq = GetFrequencySX(Rx);
    bool foundInCache = false;
    if(useCache)
    {
        int dcI, dcQ, gainI, gainQ, phOffset;
        foundInCache = (valueCache.GetDC_IQ(boardId, rxFreq, channel, false, lna, &dcI, &dcQ, &gainI, &gainQ, &phOffset) == 0);
        dcoffi = dcI;
        dcoffq = dcQ;
        mingcorri = gainI;
        mingcorrq = gainQ;
        phaseOffset = phOffset;
        if(foundInCache)
        {
            printf("Rx calibration: using cached values\n");
            SetRxDCOFF(dcoffi, dcoffq);
            Modify_SPI_Reg_bits(LMS7param(EN_DCOFF_RXFE_RFE), 1);
            Modify_SPI_Reg_bits(LMS7param(GCORRI_RXTSP), gainI);
            Modify_SPI_Reg_bits(LMS7param(GCORRQ_RXTSP), gainQ);
            Modify_SPI_Reg_bits(LMS7param(IQCORR_RXTSP), phaseOffset);
            Modify_SPI_Reg_bits(0x040C, 2, 0, 0); //DC_BYP 0, GC_BYP 0, PH_BYP 0
            Modify_SPI_Reg_bits(0x0110, 4, 0, 31); //ICT_LO_RFE 31
            return 0;
        }
    }
    LMS7002M_SelfCalState state(this);

    Log("Rx calibration started", LOG_INFO);
    Log("Saving registers state", LOG_INFO);
    BackupAllRegisters();
    if(sel_path_rfe == 1 || sel_path_rfe == 0)
        return ReportError(EINVAL, "Rx Calibration: bad SEL_PATH");

    Log("Setup stage", LOG_INFO);
    status = CalibrateRxSetup(bandwidth_Hz, useExtLoopback);
    if(status != 0)
        goto RxCalibrationEndStage;

    if(mCalibrationByMCU)
    {
        //set reference clock parameter inside MCU
        long refClk = GetReferenceClk_SX(false);
        uint16_t refClkToMCU = (int(refClk / 1000000) << 9) | ((refClk % 1000000) / 10000);
        SPI_write(MCU_PARAMETER_ADDRESS, refClkToMCU);
        mcuControl->CallMCU(MCU_FUNCTION_UPDATE_REF_CLK);
        auto statusMcu = mcuControl->WaitForMCU(100);

        //set bandwidth for MCU to read from register, value is integer stored in MHz
        SPI_write(MCU_PARAMETER_ADDRESS, (uint16_t)(bandwidth_Hz / 1e6));
        mcuControl->CallMCU(MCU_FUNCTION_CALIBRATE_RX);
        statusMcu = mcuControl->WaitForMCU(30000);
        if(statusMcu == 0)
        {
            printf("MCU working too long %i\n", statusMcu);
        }
    }
    else
    {
        Log("Rx DC calibration", LOG_INFO);

        CalibrateRxDC_RSSI();

        // RXIQ calibration
        Modify_SPI_Reg_bits(LMS7param(EN_G_TRF), 1);

        if(not useExtLoopback)
        {
            if (sel_path_rfe == 2)
            {
                Modify_SPI_Reg_bits(LMS7param(PD_RLOOPB_2_RFE), 0);
                Modify_SPI_Reg_bits(LMS7param(EN_INSHSW_LB2_RFE), 0);
            }
            if (sel_path_rfe == 3)
            {
                Modify_SPI_Reg_bits(LMS7param(PD_RLOOPB_1_RFE), 0);
                Modify_SPI_Reg_bits(LMS7param(EN_INSHSW_LB1_RFE), 0);
            }
            Modify_SPI_Reg_bits(DC_BYP_RXTSP, 0); //DC_BYP 0
        }

        Modify_SPI_Reg_bits(MAC, 2);
        if (Get_SPI_Reg_bits(PD_LOCH_T2RBUF) == false)
        {
            Modify_SPI_Reg_bits(PD_LOCH_T2RBUF, 1);
            //TDD MODE
            Modify_SPI_Reg_bits(MAC, 1);
            Modify_SPI_Reg_bits(PD_VCO, 0);
        }
        Modify_SPI_Reg_bits(MAC, ch);

        CheckSaturationRx(bandwidth_Hz, useExtLoopback);

        Modify_SPI_Reg_bits(CMIX_SC_RXTSP, 1);
        Modify_SPI_Reg_bits(CMIX_BYP_RXTSP, 0);
        SetNCOFrequency(LMS7002M::Rx, 0, bandwidth_Hz/calibUserBwDivider + 0.1e6);

        Modify_SPI_Reg_bits(IQCORR_RXTSP, 0);
        Modify_SPI_Reg_bits(GCORRI_RXTSP, 2047);
        Modify_SPI_Reg_bits(GCORRQ_RXTSP, 2047);

        //coarse gain
        {
            Modify_SPI_Reg_bits(GCORRI_RXTSP, 2047 - 64);
            Modify_SPI_Reg_bits(GCORRQ_RXTSP, 2047);
            uint32_t rssiIgain = GetRSSI();
            Modify_SPI_Reg_bits(GCORRI_RXTSP, 2047);
            Modify_SPI_Reg_bits(GCORRQ_RXTSP, 2047 - 64);
            uint32_t rssiQgain = GetRSSI();

            Modify_SPI_Reg_bits(GCORRI_RXTSP, 2047);
            Modify_SPI_Reg_bits(GCORRQ_RXTSP, 2047);

            gain = 1983;
            if(rssiIgain < rssiQgain)
            {
                gainAddr = 0x0402;
                Modify_SPI_Reg_bits(GCORRI_RXTSP, gain);
            }
            else
            {
                gainAddr = 0x0401;
                Modify_SPI_Reg_bits(GCORRQ_RXTSP, gain);
            }
        }

        CoarseSearch(gainAddr, gainMSB, gainLSB, gain, 7);
#ifdef LMS_VERBOSE_OUTPUT
        printf("Coarse search Rx GAIN_%s: %i\n", gainAddr == GCORRI_RXTSP.address ? "I" : "Q", gain);
#endif

        //find phase offset

        uint32_t rssiUp;
        uint32_t rssiDown;
        Modify_SPI_Reg_bits(IQCORR_RXTSP, 15);
        rssiUp = GetRSSI();
        Modify_SPI_Reg_bits(IQCORR_RXTSP, -15);
        rssiDown = GetRSSI();

        if(rssiUp > rssiDown)
            phaseOffset = -64;
        else if(rssiUp < rssiDown)
            phaseOffset = 192;
        else
            phaseOffset = 64;
        Modify_SPI_Reg_bits(IQCORR_RXTSP, phaseOffset);
        CoarseSearch(IQCORR_RXTSP.address, IQCORR_RXTSP.msb, IQCORR_RXTSP.lsb, phaseOffset, 7);
#ifdef LMS_VERBOSE_OUTPUT
        printf("Coarse search Rx IQCORR: %i\n", phaseOffset);
#endif
        CoarseSearch(gainAddr, gainMSB, gainLSB, gain, 4);
#ifdef LMS_VERBOSE_OUTPUT
        printf("Coarse search Rx GAIN_%s: %i\n", gainAddr == GCORRI_RXTSP.address ? "I" : "Q", gain);
#endif
        CoarseSearch(IQCORR_RXTSP.address, IQCORR_RXTSP.msb, IQCORR_RXTSP.lsb, phaseOffset, 4);
#ifdef LMS_VERBOSE_OUTPUT
        printf("Coarse search Rx IQCORR: %i\n", phaseOffset);
#endif
#ifdef LMS_VERBOSE_OUTPUT
        printf("Fine search Rx GAIN_%s/IQCORR\n", gainAddr == GCORRI_RXTSP.address ? "I" : "Q");
#endif
        FineSearch(gainAddr, gainMSB, gainLSB, gain, IQCORR_RXTSP.address, IQCORR_RXTSP.msb, IQCORR_RXTSP.lsb, phaseOffset, 7);
        Modify_SPI_Reg_bits(gainAddr, gainMSB, gainLSB, gain);
        Modify_SPI_Reg_bits(IQCORR_RXTSP.address, IQCORR_RXTSP.msb, IQCORR_RXTSP.lsb, phaseOffset);
#ifdef LMS_VERBOSE_OUTPUT
        printf("Fine search Rx GAIN_%s: %i, IQCORR: %i\n", gainAddr == GCORRI_RXTSP.address ? "I" : "Q", gain, phaseOffset);
#endif

    }
    mingcorri = Get_SPI_Reg_bits(GCORRI_RXTSP, true);
    mingcorrq = Get_SPI_Reg_bits(GCORRQ_RXTSP, true);
    dcoffi = Get_SPI_Reg_bits(DCOFFI_RFE, true);
    dcoffq = Get_SPI_Reg_bits(DCOFFQ_RFE, true);
    phaseOffset = Get_SPI_Reg_bits(IQCORR_RXTSP, true);

RxCalibrationEndStage:
    Log("Restoring registers state", LOG_INFO);
    RestoreAllRegisters();

    if (status != 0)
    {
        Log("Rx calibration failed", LOG_WARNING);
        return status;
    }
    if(useCache)
        valueCache.InsertDC_IQ(boardId, rxFreq*1e6, channel, false, lna, dcoffi, dcoffq, mingcorri, mingcorrq, phaseOffset);

    Modify_SPI_Reg_bits(LMS7param(MAC), ch);
    SetRxDCOFF((int8_t)dcoffi, (int8_t)dcoffq);
    Modify_SPI_Reg_bits(LMS7param(EN_DCOFF_RXFE_RFE), 1);
    Modify_SPI_Reg_bits(LMS7param(GCORRI_RXTSP), mingcorri);
    Modify_SPI_Reg_bits(LMS7param(GCORRQ_RXTSP), mingcorrq);
    Modify_SPI_Reg_bits(LMS7param(IQCORR_RXTSP), phaseOffset);
    Modify_SPI_Reg_bits(0x040C, 2, 0, 0); //DC_BYP 0, GC_BYP 0, PH_BYP 0
    Modify_SPI_Reg_bits(0x0110, 4, 0, 31); //ICT_LO_RFE 31
    Log("Rx calibration finished", LOG_INFO);
    return 0;
}

/** @brief Stores chip current registers state into memory for later restoration
*/
void LMS7002M::BackupAllRegisters()
{
    Channel ch = this->GetActiveChannel();
    SPI_read_batch(backupAddrs, backupRegs, sizeof(backupAddrs) / sizeof(uint16_t));
    this->SetActiveChannel(ChA); // channel A
    SPI_read_batch(backupSXAddr, backupRegsSXR, sizeof(backupRegsSXR) / sizeof(uint16_t));
    //backup GFIR3 coefficients
    GetGFIRCoefficients(LMS7002M::Rx, 2, rxGFIR3_backup, sizeof(rxGFIR3_backup)/sizeof(int16_t));
    //EN_NEXTRX_RFE could be modified in channel A
    backup0x010D = SPI_read(0x010D);
    //EN_NEXTTX_TRF could be modified in channel A
    backup0x0100 = SPI_read(0x0100);
    this->SetActiveChannel(ChB); // channel B
    SPI_read_batch(backupSXAddr, backupRegsSXT, sizeof(backupRegsSXR) / sizeof(uint16_t));
    this->SetActiveChannel(ch);
}

/** @brief Sets chip registers to state that was stored in memory using BackupAllRegisters()
*/
void LMS7002M::RestoreAllRegisters()
{
    Channel ch = this->GetActiveChannel();
    SPI_write_batch(backupAddrs, backupRegs, sizeof(backupAddrs) / sizeof(uint16_t));
    //restore GFIR3
    SetGFIRCoefficients(LMS7002M::Rx, 2, rxGFIR3_backup, sizeof(rxGFIR3_backup)/sizeof(int16_t));
    this->SetActiveChannel(ChA); // channel A
    SPI_write(0x010D, backup0x010D); //restore EN_NEXTRX_RFE
    SPI_write(0x0100, backup0x0100); //restore EN_NEXTTX_TRF
    SPI_write_batch(backupSXAddr, backupRegsSXR, sizeof(backupRegsSXR) / sizeof(uint16_t));
    this->SetActiveChannel(ChB); // channel B
    SPI_write_batch(backupSXAddr, backupRegsSXT, sizeof(backupRegsSXR) / sizeof(uint16_t));
    this->SetActiveChannel(ch);
    //reset Tx logic registers, fixes interpolator
    uint16_t x0020val = SPI_read(0x0020);
    SPI_write(0x0020, x0020val & ~0xA000);
    SPI_write(0x0020, x0020val);
}

int LMS7002M::CheckSaturationRx(const float_type bandwidth_Hz, const bool useExtLoopback)
{
    Modify_SPI_Reg_bits(CMIX_SC_RXTSP, 0);
    Modify_SPI_Reg_bits(CMIX_BYP_RXTSP, 0);
    SetNCOFrequency(LMS7002M::Rx, 0, bandwidth_Hz / calibUserBwDivider - 0.1e6);

    uint32_t rssi = GetRSSI();
#ifdef ENABLE_CALIBRATION_USING_FFT
    //use FFT bin 100 kHz for RSSI
    fftBin = 569;
    //0x0B000 = -3 dBFS

    if(useExtLoopback)
    {
        const float_type target_dBFS = -14;
        int loss_main_txpad = Get_SPI_Reg_bits(LOSS_MAIN_TXPAD_TRF);
        while (rssi < target_dBFS && loss_main_txpad > 0)
        {
            rssi = GetRSSI();
            if (rssi < target_dBFS)
                loss_main_txpad -= 1;
            if (rssi > target_dBFS)
                break;
            Modify_SPI_Reg_bits(G_RXLOOPB_RFE, loss_main_txpad);
        }

        int cg_iamp = Get_SPI_Reg_bits(CG_IAMP_TBB);
        while (rssi < target_dBFS && cg_iamp < 39)
        {
            rssi = GetRSSI();
            if (rssi < target_dBFS)
                cg_iamp += 2;
            if (rssi > target_dBFS)
                break;
            Modify_SPI_Reg_bits(CG_IAMP_TBB, cg_iamp);
        }
        return 0;
    }
#endif
    int g_rxloopb_rfe = Get_SPI_Reg_bits(G_RXLOOPB_RFE);
    while (rssi < 0x0B000 && g_rxloopb_rfe  < 15)
    {
        rssi = GetRSSI();
        if (rssi < 0x0B000)
            g_rxloopb_rfe += 2;
        if (rssi > 0x0B000)
            break;
        Modify_SPI_Reg_bits(G_RXLOOPB_RFE, g_rxloopb_rfe);
    }

    int cg_iamp = Get_SPI_Reg_bits(CG_IAMP_TBB);
    while (rssi < 0x01000 && cg_iamp < 63-6)
    {
        rssi = GetRSSI();
        if (rssi < 0x01000)
            cg_iamp += 4;
        if (rssi > 0x01000)
            break;
        Modify_SPI_Reg_bits(CG_IAMP_TBB, cg_iamp);
    }

    while (rssi < 0x0B000 && cg_iamp < 62)
    {
        rssi = GetRSSI();
        if (rssi < 0x0B000)
            cg_iamp += 2;
        Modify_SPI_Reg_bits(CG_IAMP_TBB, cg_iamp);
    }
    return 0;
}

static uint16_t toDCOffset(int16_t offset)
{
    uint16_t valToSend = 0;
    if (offset < 0)
        valToSend |= 0x40;
    valToSend |= labs(offset);
    return valToSend;
}

void LMS7002M::CoarseSearch(const uint16_t addr, const uint8_t msb, const uint8_t lsb, int16_t &value, const uint8_t maxIterations)
{
    const uint16_t DCOFFaddr = 0x010E;
    uint8_t rssi_counter = 0;
    uint32_t rssiUp;
    uint32_t rssiDown;
    int16_t upval;
    int16_t downval;
    upval = value;
    for(rssi_counter = 0; rssi_counter < maxIterations - 1; ++rssi_counter)
    {
        rssiUp = GetRSSI();
        value -= pow2(maxIterations - rssi_counter);
        Modify_SPI_Reg_bits(addr, msb, lsb, addr != DCOFFaddr ? value : toDCOffset(value));
        downval = value;
        rssiDown = GetRSSI();

        if(rssiUp >= rssiDown)
            value += pow2(maxIterations - 2 - rssi_counter);
        else
            value = value + pow2(maxIterations - rssi_counter) + pow2(maxIterations - 1 - rssi_counter) - pow2(maxIterations - 2 - rssi_counter);
        Modify_SPI_Reg_bits(addr, msb, lsb, addr != DCOFFaddr ? value : toDCOffset(value));
        upval = value;
    }
    value -= pow2(maxIterations - rssi_counter);
    rssiUp = GetRSSI();
    if(addr != DCOFFaddr)
        Modify_SPI_Reg_bits(addr, msb, lsb, value - pow2(maxIterations - rssi_counter));
    else
        Modify_SPI_Reg_bits(addr, msb, lsb, toDCOffset(value - pow2(maxIterations - rssi_counter)));
    rssiDown = GetRSSI();
    if(rssiUp < rssiDown)
        value += 1;

    Modify_SPI_Reg_bits(addr, msb, lsb, addr != DCOFFaddr ? value : toDCOffset(value));
    rssiDown = GetRSSI();

    if(rssiUp < rssiDown)
    {
        value += 1;
        Modify_SPI_Reg_bits(addr, msb, lsb, addr != DCOFFaddr ? value : toDCOffset(value));
    }
}

int LMS7002M::CheckSaturationTxRx(const float_type bandwidth_Hz, const bool useExtLoopback)
{
    if(useExtLoopback)
    {
        SetNCOFrequency(LMS7002M::Rx, 0, calibrationSXOffset_Hz - 0.1e6 + bandwidth_Hz / calibUserBwDivider);

        const float target_dBFS = -10;
        int g_tia = Get_SPI_Reg_bits(G_TIA_RFE);
        int g_lna = Get_SPI_Reg_bits(G_LNA_RFE);
        int g_pga = Get_SPI_Reg_bits(G_PGA_RBB);

        while(GetRSSI() < target_dBFS && g_lna <= 15)
        {
            g_lna += 1;
            Modify_SPI_Reg_bits(LMS7param(G_LNA_RFE), g_lna);
        }
        if(g_lna > 15)
            g_lna = 15;
        Modify_SPI_Reg_bits(LMS7param(G_LNA_RFE), g_lna);
        if(GetRSSI() >= target_dBFS)
            goto rxGainFound;

        while(GetRSSI() < target_dBFS && g_tia <= 3)
        {
            g_tia += 1;
            Modify_SPI_Reg_bits(LMS7param(EN_G_TRF), g_tia);
        }
        if(g_tia > 3)
            g_tia = 3;
        Modify_SPI_Reg_bits(LMS7param(EN_G_TRF), g_tia);
        if(GetRSSI() >= target_dBFS)
            goto rxGainFound;

        while(GetRSSI() < target_dBFS && g_pga < 6)
        {
            g_pga += 2;
            Modify_SPI_Reg_bits(LMS7param(G_PGA_RBB), g_pga);
        }
        Modify_SPI_Reg_bits(LMS7param(G_PGA_RBB), g_pga);

    rxGainFound:
        Modify_SPI_Reg_bits(LMS7param(EN_G_TRF), 0);
        Modify_SPI_Reg_bits(LMS7param(CMIX_BYP_RXTSP), 1);
        CalibrateRxDC_RSSI();
        Modify_SPI_Reg_bits(LMS7param(CMIX_BYP_RXTSP), 0);
        Modify_SPI_Reg_bits(LMS7param(EN_G_TRF), 1);

        SetNCOFrequency(LMS7002M::Rx, 0, calibrationSXOffset_Hz + 0.1e6 + bandwidth_Hz / calibUserBwDivider);
        Modify_SPI_Reg_bits(LMS7param(CMIX_SC_RXTSP), 1);

    //---------IQ calibration-----------------
        int16_t iqcorr_rx = 0;
        int16_t gain;
        uint16_t gainAddr = 0;
        const uint8_t gainMSB = 10;
        const uint8_t gainLSB = 0;
        int16_t phaseOffset;

        Modify_SPI_Reg_bits(IQCORR_RXTSP, 0);
        Modify_SPI_Reg_bits(GCORRI_RXTSP, 2047);
        Modify_SPI_Reg_bits(GCORRQ_RXTSP, 2047);

        //coarse gain
        Modify_SPI_Reg_bits(GCORRI_RXTSP, 2047 - 64);
        Modify_SPI_Reg_bits(GCORRQ_RXTSP, 2047);
        uint32_t rssiIgain = GetRSSI();
        Modify_SPI_Reg_bits(GCORRI_RXTSP, 2047);
        Modify_SPI_Reg_bits(GCORRQ_RXTSP, 2047 - 64);
        uint32_t rssiQgain = GetRSSI();

        Modify_SPI_Reg_bits(GCORRI_RXTSP, 2047);
        Modify_SPI_Reg_bits(GCORRQ_RXTSP, 2047);

        gain = 1983;
        if(rssiIgain < rssiQgain)
        {
            gainAddr = 0x0402;
            Modify_SPI_Reg_bits(GCORRI_RXTSP, gain);
        }
        else
        {
            gainAddr = 0x0401;
            Modify_SPI_Reg_bits(GCORRQ_RXTSP, gain);
        }
        CoarseSearch(gainAddr, gainMSB, gainLSB, gain, 7);
        //find phase offset
        {
            uint32_t rssiUp;
            uint32_t rssiDown;
            Modify_SPI_Reg_bits(IQCORR_RXTSP, 15);
            rssiUp = GetRSSI();
            Modify_SPI_Reg_bits(IQCORR_RXTSP, -15);
            rssiDown = GetRSSI();

            if(rssiUp > rssiDown)
                phaseOffset = -64;
            else if(rssiUp < rssiDown)
                phaseOffset = 192;
            else
                phaseOffset = 64;
            Modify_SPI_Reg_bits(IQCORR_RXTSP, phaseOffset);
        }
        CoarseSearch(IQCORR_RXTSP.address, IQCORR_RXTSP.msb, IQCORR_RXTSP.lsb, phaseOffset, 7);
        CoarseSearch(gainAddr, gainMSB, gainLSB, gain, 4);
        CoarseSearch(IQCORR_RXTSP.address, IQCORR_RXTSP.msb, IQCORR_RXTSP.lsb, phaseOffset, 4);
        FineSearch(gainAddr, gainMSB, gainLSB, gain, IQCORR_RXTSP.address, IQCORR_RXTSP.msb, IQCORR_RXTSP.lsb, phaseOffset, 7);
        Modify_SPI_Reg_bits(gainAddr, gainMSB, gainLSB, gain);
        Modify_SPI_Reg_bits(IQCORR_RXTSP.address, IQCORR_RXTSP.msb, IQCORR_RXTSP.lsb, phaseOffset);
        Modify_SPI_Reg_bits(LMS7param(CMIX_SC_RXTSP), 0);
        return 0;
    }
    //----------------------------------------
    Modify_SPI_Reg_bits(LMS7param(DC_BYP_RXTSP), 0);
    Modify_SPI_Reg_bits(LMS7param(CMIX_BYP_RXTSP), 0);
    SetNCOFrequency(LMS7002M::Rx, 0, calibrationSXOffset_Hz - 0.1e6 + (bandwidth_Hz / calibUserBwDivider) * 2);

    uint32_t rssi = GetRSSI();
    int g_pga = Get_SPI_Reg_bits(G_PGA_RBB);
    int g_rxlooop = Get_SPI_Reg_bits(G_RXLOOPB_RFE);
    while(rssi < 0x0B000 && g_rxlooop < 15)
    {
        rssi = GetRSSI();
        if(rssi < 0x0B000)
        {
            g_rxlooop += 1;
            Modify_SPI_Reg_bits(G_RXLOOPB_RFE, g_rxlooop);
        }
        else
            break;
    }
    rssi = GetRSSI();
    while(g_pga < 18 && g_rxlooop == 15 && rssi < 0x0B000)
    {
        g_pga += 1;
        Modify_SPI_Reg_bits(G_PGA_RBB, g_pga);
        rssi = GetRSSI();
    }
    Modify_SPI_Reg_bits(CMIX_BYP_RXTSP, 1);
    Modify_SPI_Reg_bits(DC_BYP_RXTSP, 1);
    return 0;
}

void LMS7002M::CalibrateTxDC_RSSI(const float_type bandwidth)
{
    Modify_SPI_Reg_bits(EN_G_TRF, 1);
    Modify_SPI_Reg_bits(CMIX_BYP_TXTSP, 0);
    Modify_SPI_Reg_bits(CMIX_BYP_RXTSP, 0);
    SetNCOFrequency(LMS7002M::Rx, 0, calibrationSXOffset_Hz - 0.1e6 + (bandwidth / calibUserBwDivider));

    int16_t corrI = 64;
    int16_t corrQ = 64;
    Modify_SPI_Reg_bits(DCCORRI_TXTSP, 64);
    Modify_SPI_Reg_bits(DCCORRQ_TXTSP, 0);

    CoarseSearch(DCCORRI_TXTSP.address, DCCORRI_TXTSP.msb, DCCORRI_TXTSP.lsb, corrI, 7);
    Modify_SPI_Reg_bits(DCCORRI_TXTSP, corrI);
    Modify_SPI_Reg_bits(DCCORRQ_TXTSP, 64);
    CoarseSearch(DCCORRQ_TXTSP.address, DCCORRQ_TXTSP.msb, DCCORRQ_TXTSP.lsb, corrQ, 7);
    Modify_SPI_Reg_bits(DCCORRQ_TXTSP, corrQ);
    CoarseSearch(DCCORRI_TXTSP.address, DCCORRI_TXTSP.msb, DCCORRI_TXTSP.lsb, corrI, 4);
    Modify_SPI_Reg_bits(DCCORRI_TXTSP, corrI);
    CoarseSearch(DCCORRQ_TXTSP.address, DCCORRQ_TXTSP.msb, DCCORRQ_TXTSP.lsb, corrQ, 4);
    Modify_SPI_Reg_bits(DCCORRQ_TXTSP, corrQ);

#ifdef LMS_VERBOSE_OUTPUT
    printf("Fine search Tx DCCORRI/DCCORRQ\n");
#endif
    FineSearch(DCCORRI_TXTSP.address, DCCORRI_TXTSP.msb, DCCORRI_TXTSP.lsb, corrI, DCCORRQ_TXTSP.address, DCCORRQ_TXTSP.msb, DCCORRQ_TXTSP.lsb, corrQ, 7);
#ifdef LMS_VERBOSE_OUTPUT
    printf("Fine search Tx DCCORRI: %i, DCCORRQ: %i\n", corrI, corrQ);
#endif
    Modify_SPI_Reg_bits(DCCORRI_TXTSP, corrI);
    Modify_SPI_Reg_bits(DCCORRQ_TXTSP, corrQ);
}

void LMS7002M::FineSearch(const uint16_t addrI, const uint8_t msbI, const uint8_t lsbI, int16_t &valueI, const uint16_t addrQ, const uint8_t msbQ, const uint8_t lsbQ, int16_t &valueQ, const uint8_t fieldSize)
{
    const uint16_t DCOFFaddr = 0x010E;
    uint32_t **rssiField = new uint32_t*[fieldSize];
    for (int i = 0; i < fieldSize; ++i)
    {
        rssiField[i] = new uint32_t[fieldSize];
        for (int q = 0; q < fieldSize; ++q)
            rssiField[i][q] = ~0;
    }

    uint32_t minRSSI = ~0;
    int16_t minI = 0;
    int16_t minQ = 0;

    for (int i = 0; i < fieldSize; ++i)
    {
        for (int q = 0; q < fieldSize; ++q)
        {
            int16_t ival = valueI + (i - fieldSize / 2);
            int16_t qval = valueQ + (q - fieldSize / 2);
            Modify_SPI_Reg_bits(addrI, msbI, lsbI, addrI != DCOFFaddr ? ival : toDCOffset(ival), true);
            Modify_SPI_Reg_bits(addrQ, msbQ, lsbQ, addrQ != DCOFFaddr ? qval : toDCOffset(qval), true);
            rssiField[i][q] = GetRSSI();
            if (rssiField[i][q] < minRSSI)
            {
                minI = ival;
                minQ = qval;
                minRSSI = rssiField[i][q];
            }
        }
    }

#ifdef LMS_VERBOSE_OUTPUT
    printf("      |");
    for (int i = 0; i < fieldSize; ++i)
        printf("%6i|", valueQ - fieldSize / 2 + i);
    printf("\n");
    for (int i = 0; i < fieldSize + 1; ++i)
        printf("------+");
    printf("\n");
    for (int i = 0; i < fieldSize; ++i)
    {
        printf("%5i |", valueI + (i - fieldSize / 2));
        for (int q = 0; q < fieldSize; ++q)
            printf("%6i.2|", rssiField[i][q]);
        printf("\n");
    }
#endif

    valueI = minI;
    valueQ = minQ;
    for (int i = 0; i < fieldSize; ++i)
        delete rssiField[i];
    delete rssiField;
}

/** @brief Loads given DC_REG values into registers
    @param tx TxTSP or RxTSP selection
    @param I DC_REG I value
    @param Q DC_REG Q value
*/
int LMS7002M::LoadDC_REG_IQ(bool tx, int16_t I, int16_t Q)
{
    if(tx)
    {
        Modify_SPI_Reg_bits(LMS7param(DC_REG_TXTSP), I);
        Modify_SPI_Reg_bits(LMS7param(TSGDCLDI_TXTSP), 0);
        Modify_SPI_Reg_bits(LMS7param(TSGDCLDI_TXTSP), 1);
        Modify_SPI_Reg_bits(LMS7param(TSGDCLDI_TXTSP), 0);
        Modify_SPI_Reg_bits(LMS7param(DC_REG_TXTSP), Q);
        Modify_SPI_Reg_bits(LMS7param(TSGDCLDQ_TXTSP), 0);
        Modify_SPI_Reg_bits(LMS7param(TSGDCLDQ_TXTSP), 1);
        Modify_SPI_Reg_bits(LMS7param(TSGDCLDQ_TXTSP), 0);
    }
    else
    {
        Modify_SPI_Reg_bits(LMS7param(DC_REG_RXTSP), I);
        Modify_SPI_Reg_bits(LMS7param(TSGDCLDI_RXTSP), 0);
        Modify_SPI_Reg_bits(LMS7param(TSGDCLDI_RXTSP), 1);
        Modify_SPI_Reg_bits(LMS7param(TSGDCLDI_RXTSP), 0);
        Modify_SPI_Reg_bits(LMS7param(DC_REG_TXTSP), Q);
        Modify_SPI_Reg_bits(LMS7param(TSGDCLDQ_RXTSP), 0);
        Modify_SPI_Reg_bits(LMS7param(TSGDCLDQ_RXTSP), 1);
        Modify_SPI_Reg_bits(LMS7param(TSGDCLDQ_RXTSP), 0);
    }
    return 0;
}

int LMS7002M::StoreDigitalCorrections(const bool isTx)
{
    const int idx = this->GetActiveChannelIndex();
    const uint32_t boardId = controlPort->GetDeviceInfo().boardSerialNumber;
    const double freq = this->GetFrequencySX(isTx);
    int band = 0; //TODO
    int dccorri, dccorrq, gcorri, gcorrq, phaseOffset;

    if (isTx)
    {
        dccorri = int8_t(Get_SPI_Reg_bits(LMS7param(DCCORRI_TXTSP))); //signed 8-bit
        dccorrq = int8_t(Get_SPI_Reg_bits(LMS7param(DCCORRQ_TXTSP))); //signed 8-bit
        gcorri = int16_t(Get_SPI_Reg_bits(LMS7param(GCORRI_TXTSP))); //unsigned 11-bit
        gcorrq = int16_t(Get_SPI_Reg_bits(LMS7param(GCORRQ_TXTSP))); //unsigned 11-bit
        phaseOffset = int16_t(Get_SPI_Reg_bits(LMS7param(IQCORR_TXTSP)) << 4) >> 4; //sign extend 12-bit
    }
    else
    {
        dccorri = 0;
        dccorrq = 0;
        gcorri = int16_t(Get_SPI_Reg_bits(LMS7param(GCORRI_RXTSP)) << 4) >> 4;
        gcorrq = int16_t(Get_SPI_Reg_bits(LMS7param(GCORRQ_RXTSP)) << 4) >> 4;
        phaseOffset = int16_t(Get_SPI_Reg_bits(LMS7param(IQCORR_RXTSP)) << 4) >> 4;
    }

    return valueCache.InsertDC_IQ(boardId, freq, idx, isTx, band, dccorri, dccorrq, gcorri, gcorrq, phaseOffset);
}

int LMS7002M::ApplyDigitalCorrections(const bool isTx)
{
    const int idx = this->GetActiveChannelIndex();
    const uint32_t boardId = controlPort->GetDeviceInfo().boardSerialNumber;
    const double freq = this->GetFrequencySX(isTx);
    int band = 0; //TODO

    int dccorri, dccorrq, gcorri, gcorrq, phaseOffset;
    int rc = valueCache.GetDC_IQ_Interp(boardId, freq, idx, isTx, band, &dccorri, &dccorrq, &gcorri, &gcorrq, &phaseOffset);
    if (rc != 0) return rc;

    if (isTx)
    {
        Modify_SPI_Reg_bits(DCCORRI_TXTSP, dccorri);
        Modify_SPI_Reg_bits(DCCORRQ_TXTSP, dccorrq);
        Modify_SPI_Reg_bits(GCORRI_TXTSP, gcorri);
        Modify_SPI_Reg_bits(GCORRQ_TXTSP, gcorrq);
        Modify_SPI_Reg_bits(IQCORR_TXTSP, phaseOffset);

        Modify_SPI_Reg_bits(DC_BYP_TXTSP, 0);
        Modify_SPI_Reg_bits(PH_BYP_TXTSP, 0);
        Modify_SPI_Reg_bits(GC_BYP_TXTSP, 0);
    }
    else
    {
        Modify_SPI_Reg_bits(GCORRI_RXTSP, gcorri);
        Modify_SPI_Reg_bits(GCORRQ_RXTSP, gcorrq);
        Modify_SPI_Reg_bits(IQCORR_RXTSP, phaseOffset);

        Modify_SPI_Reg_bits(PH_BYP_RXTSP, 0);
        Modify_SPI_Reg_bits(GC_BYP_RXTSP, 0);
    }
    return 0;
}
