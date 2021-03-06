#pragma once

#include "../Wallet.h"

#include <Crypto/SecretKey.h>
#include <Wallet/Slate.h>
#include <optional>

class ReceiveSlateBuilder
{
public:
	std::unique_ptr<Slate> AddReceiverData(Wallet& wallet, const SecretKey& masterSeed, const Slate& slate, const std::optional<std::string>& messageOpt) const;

private:
	bool VerifySlateStatus(Wallet& wallet, const SecretKey& masterSeed, const Slate& slate) const;
	void AddParticipantData(Slate& slate, const SecretKey& secretKey, const SecretKey& secretNonce, const std::optional<std::string>& messageOpt) const;
	bool UpdateDatabase(Wallet& wallet, const SecretKey& masterSeed, const Slate& slate, const OutputData& outputData, const SlateContext& context) const;
};