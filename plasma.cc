
// Perhaps first try to get 256-bit values to work

#include <iostream>
#include <boost/multiprecision/cpp_int.hpp>
#include <stdio.h>
#include <map>
#include "keccak-tiny.h"
#include <secp256k1_recovery.h>

using u256 = boost::multiprecision::number<boost::multiprecision::cpp_int_backend<256, 256, boost::multiprecision::unsigned_magnitude, boost::multiprecision::unchecked, void>>;
using s256 = boost::multiprecision::number<boost::multiprecision::cpp_int_backend<256, 256, boost::multiprecision::signed_magnitude, boost::multiprecision::unchecked, void>>;  

u256 get_bytes32(FILE *f) {
    uint8_t *res = (uint8_t*)malloc(32);
    int ret = fread(res, 1, 32, f);
    printf("Got %i\n", ret);
    if (ret != 32) {
        printf("Error %i: %s\n", ferror(f), strerror(ferror(f)));
        free(res);
        return 0;
    }
    u256 x;
    for (int i = 0; i < 32; i++) {
        x = x*256;
        x += res[i];
    }
    return x;
}

struct tr {
    u256 s, r;
    uint8_t v;
    u256 from;
    u256 to;
    u256 value;
};

struct Pending {
    u256 to;
    u256 value;
    u256 block; // block number
    Pending(u256 a, u256 b, u256 c) {
        to = a; value = b; block = c;
    }
    Pending() {
    }
    Pending(Pending const &p) {
        to = p.to;
        value = p.value;
        block = p.block;
    }
};

std::map<u256, u256> balances;
std::map<u256, u256> nonces;
std::map<u256, u256> block_hash;
std::map<u256, Pending> pending;

u256 block_number;

void finalize() {
    // open file for writing
    FILE *f = fopen("state.data", "rb");
    // output balances
    // output pending
    // output block hashes <-- old hashes could be removed
    // also hash that was calculated for the state that was read
}

// well there are two modes, one is for transaction files, not all commands are allowed there

u256 max(u256 a, u256 b) {
    return a > b ? a : b;
}

std::vector<uint8_t> keccak256_v(std::vector<uint8_t> data) {
	// std::string out(32, 0);
	std::vector<uint8_t> out(32, 0);
	keccak::sha3_256(out.data(), 32, data.data(), data.size());
	return out;
}

std::vector<uint8_t> toBigEndian(u256 a) {
    std::vector<uint8_t> res(32, 0);
    for (auto i = res.size(); i != 0; a >>= 8, i--) {
		res[i-1] = (uint8_t)a & 0xff;
	}
    return res;
}

u256 fromBigEndian(std::vector<uint8_t> const &str) {
	u256 ret(0);
	for (auto i: str) ret = ((ret << 8) | (u256)i);
	return ret;
}

u256 fromBigEndian(std::vector<uint8_t>::iterator a, std::vector<uint8_t>::iterator b) {
	u256 ret(0);
    while (a != b) {
        ret = ((ret << 8) | (u256)*a);
        a++;
    }
	return ret;
}

u256 keccak256(std::vector<uint8_t> str) {
    return fromBigEndian(keccak256_v(str));
}

u256 keccak256(u256 a) {
    return keccak256(toBigEndian(a));
}

u256 keccak256(u256 a, u256 b) {
    std::vector<uint8_t> aa = toBigEndian(a);
    std::vector<uint8_t> bb = toBigEndian(b);
    aa.insert(std::end(aa), std::begin(bb), std::end(bb));
    return keccak256(aa);
}

u256 keccak256(u256 a, u256 b, u256 c) {
    std::vector<uint8_t> aa = toBigEndian(a);
    std::vector<uint8_t> bb = toBigEndian(b);
    std::vector<uint8_t> cc = toBigEndian(c);
    aa.insert(std::end(aa), std::begin(bb), std::end(bb));
    aa.insert(std::end(aa), std::begin(cc), std::end(cc));
    return keccak256(aa);
}

u256 keccak256(u256 a, u256 b, u256 c, u256 d) {
    std::vector<uint8_t> aa = toBigEndian(a);
    std::vector<uint8_t> bb = toBigEndian(b);
    std::vector<uint8_t> cc = toBigEndian(c);
    std::vector<uint8_t> dd = toBigEndian(d);
    aa.insert(std::end(aa), std::begin(bb), std::end(bb));
    aa.insert(std::end(aa), std::begin(cc), std::end(cc));
    aa.insert(std::end(aa), std::begin(dd), std::end(dd));
    return keccak256(aa);
}

secp256k1_context const* getCtx() {
	static std::unique_ptr<secp256k1_context, decltype(&secp256k1_context_destroy)> s_ctx{
		secp256k1_context_create(SECP256K1_CONTEXT_SIGN | SECP256K1_CONTEXT_VERIFY),
		&secp256k1_context_destroy
	};
	return s_ctx.get();
}

u256 ecrecover(std::vector<uint8_t> const& _sig, std::vector<uint8_t> _message) {
	int v = _sig[64];
	if (v > 3) return 0;

	auto* ctx = getCtx();
	secp256k1_ecdsa_recoverable_signature rawSig;
	if (!secp256k1_ecdsa_recoverable_signature_parse_compact(ctx, &rawSig, _sig.data(), v))
		return 0;

	secp256k1_pubkey rawPubkey;
	if (!secp256k1_ecdsa_recover(ctx, &rawPubkey, &rawSig, _message.data()))
		return 0;

	std::array<uint8_t, 65> serializedPubkey;
	size_t serializedPubkeySize = serializedPubkey.size();
	secp256k1_ec_pubkey_serialize(
			ctx, serializedPubkey.data(), &serializedPubkeySize,
			&rawPubkey, SECP256K1_EC_UNCOMPRESSED
	);
	assert(serializedPubkeySize == serializedPubkey.size());
	// Expect single byte header of value 0x04 -- uncompressed public key.
	assert(serializedPubkey[0] == 0x04);
	// Create the Public skipping the header.
    
    std::vector<uint8_t> out(32, 0);
	keccak::sha3_256(out.data(), 32, &serializedPubkey[1], 64);
	return fromBigEndian(out.begin()+12, out.end());
}

void process(FILE *f, u256 hash) {
    u256 control = get_bytes32(f);
    if (control == 0) {
        fclose(f);
        finalize();
        exit(0);
    }
    // Balance, nonce
    else if (control == 1) {
        u256 addr = get_bytes32(f);
        u256 v = get_bytes32(f);
        u256 nonce = get_bytes32(f);
        balances[addr] = v;
        nonces[addr] = nonce;
    }
    // Pending transaction
    else if (control == 2) {
        u256 from = get_bytes32(f);
        u256 to = get_bytes32(f);
        u256 value = get_bytes32(f);
        u256 block = get_bytes32(f);
        pending[from] = Pending(to, value, block);
    }
    // Block hash
    else if (control == 3) {
        u256 num = get_bytes32(f);
        u256 hash = get_bytes32(f);
        block_hash[num] = hash;
        block_number = max(num+1, block_number);
    }
    // Transaction: remove from account, add to pending
    else if (control == 4) {
        u256 from = get_bytes32(f);
        u256 to = get_bytes32(f);
        u256 value = get_bytes32(f);
        u256 nonce = get_bytes32(f);
        u256 r = get_bytes32(f);
        u256 s = get_bytes32(f);
        u256 v = get_bytes32(f);
        // TODO: Check signature
        u256 hash = keccak256(from, to, value, nonce);
        u256 bal = balances[from];
        if (bal < v || nonces[from] != nonce || pending.find(from) == pending.end()) return;
        balances[from] = bal - v;
        nonces[from]++;
        pending[from] = Pending(to, value, block_number);
    }
    // Confirm transaction
    else if (control == 5) {
        u256 from = get_bytes32(f);
        u256 hash = get_bytes32(f);
        Pending p = pending[from];
        if (block_hash[p.block] != hash) return;
        balances[p.to] = p.value;
        pending.erase(from);
    }
}

// first thing is calculating the hash of the state

int main(int argc, char **argv) {
    u256 x;
    std::string t1("asd");
    std::string t2("bsd");
    x++;
    std::cout << "Checking 256-bit values: " << x << std::endl;
    /*
    FILE *f = fopen("state.data", "rb");
    process(f);
    */
    return 0;
}
