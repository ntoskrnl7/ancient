/* Copyright (C) Teemu Suutari */

#include "HFMNDecompressor.hpp"
#include "HuffmanDecoder.hpp"
#include "InputStream.hpp"
#include "OutputStream.hpp"

bool HFMNDecompressor::detectHeaderXPK(uint32_t hdr) noexcept
{
	return hdr==FourCC('HFMN');
}

std::unique_ptr<XPKDecompressor> HFMNDecompressor::create(uint32_t hdr,uint32_t recursionLevel,const Buffer &packedData,std::unique_ptr<XPKDecompressor::State> &state,bool verify)
{
	return std::make_unique<HFMNDecompressor>(hdr,recursionLevel,packedData,state,verify);
}

HFMNDecompressor::HFMNDecompressor(uint32_t hdr,uint32_t recursionLevel,const Buffer &packedData,std::unique_ptr<XPKDecompressor::State> &state,bool verify) :
	XPKDecompressor(recursionLevel),
	_packedData(packedData)
{
	if (!detectHeaderXPK(hdr) || packedData.size()<4)
		throw Decompressor::InvalidFormatError();
	uint16_t tmp=packedData.readBE16(0);
	if (tmp&3) throw Decompressor::InvalidFormatError();	// header is being written in 4 byte chunks
	_headerSize=size_t(tmp&0x1ff);				// the top 7 bits are flags. No definition what they are and they are ignored in decoder...
	if (size_t(_headerSize)+4>packedData.size()) throw Decompressor::InvalidFormatError();
	tmp=packedData.readBE16(_headerSize+2);
	_rawSize=size_t(tmp);
	if (!_rawSize) throw Decompressor::InvalidFormatError();
	_headerSize+=4;
}

HFMNDecompressor::~HFMNDecompressor()
{
	// nothing needed
}

const std::string &HFMNDecompressor::getSubName() const noexcept
{
	static std::string name="XPK-HFMN: Huffman compressor";
	return name;
}

void HFMNDecompressor::decompressImpl(Buffer &rawData,const Buffer &previousData,bool verify)
{
	if (rawData.size()!=_rawSize) throw Decompressor::DecompressionError();
	ForwardInputStream inputStream(_packedData,2,_headerSize);
	MSBBitReader<ForwardInputStream> bitReader(inputStream);
	auto readBit=[&]()->uint32_t
	{
		return bitReader.readBits8(1);
	};

	ForwardOutputStream outputStream(rawData,0,rawData.size());

	HuffmanDecoder<uint32_t> decoder;
	uint32_t code=1;
	uint32_t codeBits=1;
	for (;;)
	{
		if (!readBit())
		{
			uint32_t lit=0;
			for (uint32_t i=0;i<8;i++) lit|=readBit()<<i;
			decoder.insert(HuffmanCode<uint32_t>{codeBits,code,lit});
			while (!(code&1) && codeBits)
			{
				codeBits--;
				code>>=1;
			}
			if (!codeBits) break;	
			code--;
		} else {
			code=(code<<1)+1;
			codeBits++;
		}
	}
	inputStream=ForwardInputStream(_packedData,_headerSize,_packedData.size());
	bitReader.reset();

	while (!outputStream.eof())
		outputStream.writeByte(decoder.decode(readBit));
}

XPKDecompressor::Registry<HFMNDecompressor> HFMNDecompressor::_XPKregistration;
