from web3 import Web3

provider_rpc = {
    "development": "https://mainnet.infura.io/v3/bbac679712f44fd287c1f59293eee32a",
    "alphanet": "https://rpc.api.moonbase.moonbeam.network",
}
web3 = Web3(Web3.HTTPProvider(provider_rpc["development"]))  # Change to correct network

address_from = "0x8997b145B302A13bFf57d3084DCD74f3e890F6C2"
address_to = "0x8997b145B302A13bFf57d3084DCD74f3e890F6C2"

balance_from = web3.fromWei(web3.eth.getBalance(address_from), "ether")
balance_to = web3.fromWei(web3.eth.getBalance(address_to), "ether")

print(f"The balance of { address_from } is: { balance_from } ETH")
print(f"The balance of { address_to } is: { balance_to } ETH")
