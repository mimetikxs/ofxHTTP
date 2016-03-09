// =============================================================================
//
// Copyright (c) 2013-2016 Christopher Baker <http://christopherbaker.net>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//
// =============================================================================


#include "ofx/HTTP/JSONWebToken.h"
#include "Poco/Crypto/RSAKey.h"
#include "Poco/Crypto/RSADigestEngine.h"
#include "ofx/IO/Base64Encoding.h"
#include "ofx/IO/ByteBuffer.h"
#include "ofLog.h"


namespace ofx {
namespace HTTP {


JSONWebTokenData::JSONWebTokenData()
{
}


JSONWebTokenData::~JSONWebTokenData()
{
}


std::string JSONWebTokenData::asBase64URLEncodedString() const
{
    return IO::Base64Encoding::encode(_data.dump(), true);
}


std::string JSONWebTokenData::asString(int indent) const
{
    return _data.dump(indent);
}


const ofJson& JSONWebTokenData::data() const
{
    return _data;
}


void JSONWebTokenData::clear(const std::string& name)
{
    _data.erase(_data.find(name));
}
    

const std::string JSONWebTokenHeader::TYP = "typ";
const std::string JSONWebTokenHeader::CTY = "cty";


JSONWebTokenHeader::JSONWebTokenHeader()
{
}


JSONWebTokenHeader::~JSONWebTokenHeader()
{
}


void JSONWebTokenHeader::setType(const std::string& type)
{
    set(TYP, type);
}


void JSONWebTokenHeader::setContentType(const std::string& contentType)
{
    set(CTY, contentType);
}


const std::string JSONWebSignatureHeader::ALG = "alg";
const std::string JSONWebSignatureHeader::JKU = "jku";
const std::string JSONWebSignatureHeader::JWK = "jwk";
const std::string JSONWebSignatureHeader::KID = "kid";
const std::string JSONWebSignatureHeader::X5U = "x5u";
const std::string JSONWebSignatureHeader::X5T = "x5t";
const std::string JSONWebSignatureHeader::X5C = "x5c";
const std::string JSONWebSignatureHeader::CRIT = "crit";


JSONWebSignatureHeader::JSONWebSignatureHeader()
{
}


JSONWebSignatureHeader::~JSONWebSignatureHeader()
{
}


void JSONWebSignatureHeader::setAlgorithm(Algorithm algorithm)
{
    set(ALG, toString(algorithm));
}


void JSONWebSignatureHeader::setKeyId(const std::string& keyId)
{
    set(KID, keyId);
}


std::string JSONWebSignatureHeader::toString(Algorithm algorithm)
{
    switch (algorithm)
    {
        case Algorithm::RS256:
            return "RS256";
        default:
            return "UNKNOWN";
    }
}


const std::string JSONWebTokenPayload::ISS = "iss";
const std::string JSONWebTokenPayload::AUD = "aud";
const std::string JSONWebTokenPayload::JTI = "jti";
const std::string JSONWebTokenPayload::IAT = "iat";
const std::string JSONWebTokenPayload::EXP = "exp";
const std::string JSONWebTokenPayload::NBF = "nbf";
const std::string JSONWebTokenPayload::TYP = "typ";
const std::string JSONWebTokenPayload::SUB = "sub";
//const std::string JSONWebTokenPayload::SCOPE = "scope";


JSONWebTokenPayload::JSONWebTokenPayload()
{
}


JSONWebTokenPayload::~JSONWebTokenPayload()
{
}


void JSONWebTokenPayload::setIssuer(const std::string& issuer)
{
    set(ISS, issuer);
}


void JSONWebTokenPayload::setAudience(const std::vector<std::string>& audience)
{
    // Find remove duplicate scopes.
    std::vector<std::string> _audience = audience;
    auto it = std::unique(_audience.begin(), _audience.end());
    _audience.resize(std::distance(_audience.begin(), it));

    // Join with white spaces.
    std::stringstream ss;

    for(size_t i = 0; i < _audience.size(); ++i)
    {
        if(i != 0)
        {
            ss << " ";
        }
        
        ss << _audience[i];
    }
    
    setAudience(ss.str());
}


void JSONWebTokenPayload::setAudience(const std::string& audience)
{
    set(AUD, audience);
}


void JSONWebTokenPayload::setId(const std::string& id)
{
    set(JTI, id);
}


void JSONWebTokenPayload::setIssuedAtTime(uint64_t time)
{
    set(IAT, time);
}


void JSONWebTokenPayload::setExpirationTime(uint64_t time)
{
    set(EXP, time);
}



void JSONWebTokenPayload::setNotBeforeTime(uint64_t time)
{
    set(NBF, time);
}


void JSONWebTokenPayload::setType(const std::string& type)
{
    set(TYP, type);
}



void JSONWebTokenPayload::setSubject(const std::string& subject)
{
    set(SUB, subject);
}


std::string JSONWebToken::generateToken(const std::string& privateKey,
                                        const std::string& privateKeyPassphrase,
                                        const JSONWebSignatureHeader& header,
                                        const JSONWebTokenPayload& payload)
{
    if (header.data()[JSONWebSignatureHeader::ALG].is_string())
    {
        std::string alg = header.data()[JSONWebSignatureHeader::ALG].get<std::string>();

        if (alg.compare("RS256") != 0)
        {
            ofLogError("JSONWebToken::generateToken") << "Signature algorithm: " << alg << " not supported.";
            return "";
        }
    }
    else
    {
        ofLogError("JSONWebToken::generateToken") << "No signature algorithm selected.";
        return "";
    }

    std::string encodedHeaderAndClaims;

    encodedHeaderAndClaims += header.asBase64URLEncodedString();
    encodedHeaderAndClaims += ".";
    encodedHeaderAndClaims += payload.asBase64URLEncodedString();

    std::istringstream _privateKey(privateKey);

    Poco::Crypto::RSAKey key(nullptr,
                             &_privateKey,
                             privateKeyPassphrase);

    Poco::Crypto::RSADigestEngine digestEngine(key, "SHA256");

    digestEngine.update(encodedHeaderAndClaims.data(),
                        encodedHeaderAndClaims.size());

    const Poco::DigestEngine::Digest& signature = digestEngine.signature();

    ofx::IO::ByteBuffer signatureBuffer(signature);
    ofx::IO::ByteBuffer encodedSignatureBuffer;

    ofx::IO::Base64Encoding encoder(true);
    encoder.encode(signatureBuffer, encodedSignatureBuffer);

    return encodedHeaderAndClaims + "." + encodedSignatureBuffer.getText();
}


} } // namespace ofx::HTTP
