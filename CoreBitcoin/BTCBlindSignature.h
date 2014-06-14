// CoreBitcoin by Oleg Andreev <oleganza@gmail.com>, WTFPL.
// Implementation of blind signatures for Bitcoin transactions:
// http://oleganza.com/blind-ecdsa-draft-v2.pdf

#import <Foundation/Foundation.h>

@class BTCKey;
@class BTCKeychain;
@class BTCBigNumber;
@class BTCCurvePoint;
@interface BTCBlindSignature : NSObject

// Convenience API
// This is BIP32-based API to keep track of just a single private key for multiple signatures.
//
// 1. Alice creates an extended private key u.
// 2. Bob creates an extended private key w and sends the corresponding extended public key W to Alice.
// 3. Alice assigns an index i for each “blind” public key T to use in the future. Alice must use each value of i only for one message.
// 4. Alice generates a, b, c, d using HD(u, i), a “hardened” derivation according to BIP32:
//    a = HD(u, 4·i + 0)
//    b = HD(u, 4·i + 1)
//    c = HD(u, 4·i + 2)
//    d = HD(u, 4·i + 3)
// 5. Alice generates P and Q via ND(W, i), normal (“non-hardened”) derivation according to BIP32:
//    P = ND(W, 2·i + 0)
//    Q = ND(W, 2·i + 1)
// 6. Using a, b, c, d, P and Q, Alice computes T and K and stores tuple (T, K, i) for the future,
//    until she needs to request a signature from Bob.
// 7. When Alice needs to sign her message, she retrieves K and i
//    and recovers her secret parameters a, b, c, d using HD(u, i).
// 8. Alice sends Bob a blinded hash h2 = a·h + b (mod n) and index i.
// 9. Because P and Q are not simply the public keys of p and q,
//    Bob needs to do extra operations to derive p and q from w and i (following BIP32 would not be enough):
//    From
//      P = p^-1·G = (w + x)·G (where x is a factor in ND(W, 2·i + 0))
//    follows:
//      p = (w + x)^-1 mod n
//
//    From
//      Q = q·p^-1·G = (w + y)·G (where y is a factor in ND(W, 2·i + 1))
//    follows:
//      q = (w + y)·(w + x)^-1 mod n
//
//    Factors x and y are produced according to BIP32 as first 32 bytes of HMAC-SHA512.
//    See [BIP32] for details.
//
// 10. Bob computes blinded signature s1 = p·h2 + q (mod n) and sends it to Alice (after verifying her identity).
// 11. Alice receives the blinded signature, unblinds it and arrives at a final signature (Kx, s2).

// Alice as a client needs private client keychain and public custodian keychain (provided by Bob).
- (id) initWithClientKeychain:(BTCKeychain*)clientKeychain custodianKeychain:(BTCKeychain*)custodianKeychain;

// Bob as a custodian only needs his own private keychain.
- (id) initWithCustodianKeychain:(BTCKeychain*)custodianKeychain;

// Steps 4-6: Alice generates public key to use in a transaction.
- (BTCKey*) publicKeyAtIndex:(uint32_t)index;

// Steps 7-8: Alice blinds her message and sends it to Bob.
- (NSData*) blindedHashForHash:(NSData*)hash index:(uint32_t)index;

// Step 9-10: Bob computes a signature for Alice.
- (NSData*) blindSignatureForBlindedHash:(NSData*)blindedHash index:(uint32_t)index;

// Step 11: Alice receives signature from Bob and generates final DER-encoded signature to use in transaction.
// Note: Do not forget to add SIGHASH byte in the end when placing in a Bitcoin transaction.
- (NSData*) unblindedSignatureForBlindSignature:(NSData*)blindSignature index:(uint32_t)index;


// Core Algorithm
// Exposed as a public API for testing purposes. Use less verbose convenience API above for real usage.

// Alice wants Bob to sign her transactions blindly.
// Bob will provide Alice with blinded public keys and blinded signatures for each transaction.

// 1. Alice chooses random numbers a, b, c, d within [1, n – 1].
// 2. Bob chooses random numbers p, q within [1, n – 1]  
//    and sends two EC points to Alice: 
//    P = (p^-1·G) and Q = (q·p^-1·G).
// 3. Alice computes K = (c·a)^-1·P and public key T = (a·Kx)^-1·(b·G + Q + d·c^-1·P).
//    Bob cannot know if his parameters were involved in K or T without the knowledge of a, b, c and d. 
//    Thus, Alice can safely publish T (e.g. in a Bitcoin transaction that locks funds with T).
// 4. When time comes to sign a message (e.g. redeeming funds locked in a Bitcoin transaction), 
//    Alice computes the hash h of her message.
// 5. Alice blinds the hash and sends h2 = a·h + b (mod n) to Bob.
// 6. Bob verifies the identity of Alice via separate communications channel.
// 7. Bob signs the blinded hash and returns the signature to Alice: s1 = p·h2 + q (mod n).
// 8. Alice unblinds the signature: s2 = c·s1 + d (mod n).
// 9. Now Alice has (Kx, s2) which is a valid ECDSA signature of hash h verifiable by public key T. 
//    If she uses it in a Bitcoin transaction, she will be able to redeem her locked funds without Bob knowing which transaction he just helped to sign.

// Step 2: P and Q from Bob as blinded "public keys". Both P and C are BTCCurvePoint instances.
- (NSArray*) bob_P_and_Q_for_p:(BTCBigNumber*)p q:(BTCBigNumber*)q;

// 3. Alice computes K = (c·a)^-1·P and public key T = (a·Kx)^-1·(b·G + Q + d·c^-1·P).
//    Bob cannot know if his parameters were involved in K or T without the knowledge of a, b, c and d.
- (NSArray*) alice_K_and_T_for_a:(BTCBigNumber*)a b:(BTCBigNumber*)b c:(BTCBigNumber*)c d:(BTCBigNumber*)d P:(BTCCurvePoint*)P Q:(BTCCurvePoint*)Q;

// 5. Alice blinds the hash and sends h2 = a·h + b (mod n) to Bob.
- (BTCBigNumber*) aliceBlindedHashForHash:(BTCBigNumber*)hash a:(BTCBigNumber*)a b:(BTCBigNumber*)b;

// 7. Bob signs the blinded hash and returns the signature to Alice: s1 = p·h2 + q (mod n).
- (BTCBigNumber*) bobBlindedSignatureForHash:(BTCBigNumber*)hash p:(BTCBigNumber*)p q:(BTCBigNumber*)q;

// 8. Alice unblinds the signature: s2 = c·s1 + d (mod n).
- (BTCBigNumber*) aliceUnblindedSignatureForSignature:(BTCBigNumber*)blindSignature c:(BTCBigNumber*)c d:(BTCBigNumber*)d;

// 9. Now Alice has (Kx, s2) which is a valid ECDSA signature of hash h verifiable by public key T.
// This returns final DER-encoded ECDSA signature ready to be used in a bitcoin transaction.
// Note: Do not forget to add SIGHASH byte in the end when placing in a Bitcoin transaction.
- (NSData*) aliceCompleteSignatureForKx:(BTCBigNumber*)Kx unblindedSignature:(BTCBigNumber*)unblindedSignature;

@end
