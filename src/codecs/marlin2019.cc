#include <marlin/inc/marlin.h>

#include <codecs/marlin2019.hpp>
#include <util/distribution.hpp>

#include <functional>
#include <string>
#include <iostream>
#include <cassert>
#include <cstring>
#include <stack>
#include <queue>
#include <map>
#include <bitset>
#include <unordered_map>
#include <algorithm>
#include <memory>

struct Marlin2019Pimpl : public CODEC8Z {
	
	std::vector<std::shared_ptr<Marlin>> dictionaries;
	
	std::string coderName;
	std::string name() const { return coderName; }
	
	
	Marlin2019Pimpl(Distribution::Type distType, std::map<std::string, double> conf) {

		conf.emplace("numDict", 11);

		{
			std::ostringstream oss;
			oss << "Marlin2019";
			for (auto &&c : conf)
				oss << " " << c.first << ":" << c.second;
			coderName = oss.str();
		}
		

		std::vector<std::shared_ptr<Marlin>> builtDictionaries(conf["numDict"]);

//		#pragma omp parallel for
		for (size_t p=0; p<builtDictionaries.size(); p++) {
			
			std::vector<double> pdf(256,0.);
			
			size_t nSamples = 10;
			for (double i=0.5/nSamples; i<0.9999999; i+=1./nSamples) {
				auto pdf0 = Distribution::pdf(distType, (p+i)/builtDictionaries.size());
				for (size_t j=0; j<pdf.size(); j++)
					pdf[j] += pdf0[j]/nSamples;
			}

			builtDictionaries[p] = std::make_shared<Marlin>("",pdf,conf);
			
			auto testData = Distribution::getResiduals(pdf, 1<<20);
			std::vector<uint8_t> out(testData.size());
			ssize_t sz = builtDictionaries[p]->compress(testData, out);
			//printf("%lf %lf\n", ((p+0.5)/builtDictionaries.size())/(double(sz)/testData.size()), builtDictionaries[p]->efficiency);
		}
		
		dictionaries.resize(256);
		
//		#pragma omp parallel for
		for (size_t h=0; h<256; h+=4) {
			
			auto testData = Distribution::getResiduals(Distribution::pdf(distType, (h+2)/256.), 1<<16);
			
			//std::cout << "Test h: " << h << std::endl;
			double lowestSize = testData.size()*0.99; // If efficiency is not enough to compress 1%, skip compression
			for (auto &&dict : builtDictionaries) {
				std::vector<uint8_t> out(testData.size());
				ssize_t sz = dict->compress(testData, out);
				out.resize(sz);
				//std::cout << sz << std::endl;
				
				if (out.size() < lowestSize) {
					//std::cout << "Selected!" << std::endl;
					lowestSize = out.size();
					for (size_t hh = 0; hh<4; hh++)
						dictionaries[h+hh] = dict;
				}
			}	
		}
	}

	
	void   compress(
		const std::vector<std::reference_wrapper<const AlignedArray8>> &in,
		      std::vector<std::reference_wrapper<      AlignedArray8>> &out,
		      std::vector<std::reference_wrapper<      uint8_t      >> &entropy) const { 
		
		for (size_t i=0; i<in.size(); i++)
			if (dictionaries[entropy[i]]) {
				//std::cout << "Compressing " << in[i].get().size() << " to: " << out[i].get().size() << std::endl;
				size_t sz = dictionaries[entropy[i]]->compress(
					marlin::make_view((const uint8_t *)in[i].get().begin(), (const uint8_t *)in[i].get().end()),
					marlin::make_view(out[i].get().begin(), out[i].get().begin() + out[i].get().capacity()) 
				);
				//std::cout << "Compressed " << in[i].get().size() << " to: " << sz << std::endl;
				out[i].get().resize(sz);
			} else {
				out[i].get().resize(in[i].get().size());
			}
	}

	void uncompress(
		const std::vector<std::reference_wrapper<const AlignedArray8>> &in,
		      std::vector<std::reference_wrapper<      AlignedArray8>> &out,
		      std::vector<std::reference_wrapper<const uint8_t      >> &entropy) const {
		
		for (size_t i=0; i<in.size(); i++)
			if (dictionaries[entropy[i]])
				out[i].get().resize(dictionaries[entropy[i]]->decompress(
					marlin::make_view((const uint8_t *)in[i].get().begin(), (const uint8_t *)in[i].get().end()),
					marlin::make_view(out[i].get().begin(), out[i].get().end()) 
				));
			else
				out[i].get().resize(in[i].get().size());
	}

};


	
Marlin2019::Marlin2019(Distribution::Type distType, std::map<std::string, double> conf) 
	: CODEC8withPimpl( new Marlin2019Pimpl(distType, conf) ) {}

