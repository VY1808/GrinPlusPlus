#include <ThirdParty/Catch2/catch.hpp>

#include <Wallet/WalletManager.h>
#include <Config/ConfigManager.h>
#include <uuid.h>

class TestNodeClient : public INodeClient
{
public:
	virtual uint64_t GetChainHeight() const override final { return 0; }
	virtual std::map<Commitment, OutputLocation> GetOutputsByCommitment(const std::vector<Commitment>& commitments) const override final { return std::map<Commitment, OutputLocation>(); }
};

TEST_CASE("WalletServer::InitiailizeNewWallet")
{
	Config config = ConfigManager::LoadConfig();
	TestNodeClient nodeClient;
	IWalletManager* pWalletManager = WalletAPI::StartWalletManager(config, nodeClient);

	const std::string username = uuids::to_string(uuids::uuid_system_generator()());
	SecureString words = pWalletManager->InitializeNewWallet(username, "Password1");
	REQUIRE(!words.empty());
}