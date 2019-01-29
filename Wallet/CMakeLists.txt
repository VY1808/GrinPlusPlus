set(TARGET_NAME Wallet)

# Wallet
file(GLOB Wallet_SRC
	"Wallet.cpp"
	"Sender.cpp"
)

add_subdirectory(Keychain)

add_library(${TARGET_NAME} SHARED ${Wallet_SRC})
target_compile_definitions(${TARGET_NAME} PRIVATE MW_Wallet)

add_dependencies(${TARGET_NAME} Infrastructure Crypto Core Keychain)
target_link_libraries(${TARGET_NAME} Infrastructure Crypto Core Keychain)