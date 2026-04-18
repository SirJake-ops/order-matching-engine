//
// Created by jake on 3/10/26.
//

#ifndef TRADINGEXCHANGE_EVENT_TYPES_H
#define TRADINGEXCHANGE_EVENT_TYPES_H
#include <cstdint>
#include <string>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/serialization/string.hpp>
#include <sodium.h>

// TODO: The code below is for learning purposes only, this is pulled from the boost.org website for signing a transaction for a crypto wallet
namespace event_types {
    struct Transaction {
        std::string &_sender;
        std::string &_receiver;
        uint64_t _amount;
        uint64_t _timestamp;
        std::string _symbol;
        boost::uuids::uuid _transaction_id;


        template<typename T>
        void serialize(T &t, const uint8_t version) {
            t & _sender;
            t & _receiver;
            t & _amount;
            t & _timestamp;
            t & _symbol;
            t & _transaction_id;
        }
    };

    std::string serialize_transaction(const Transaction &transaction) {
        std::ostringstream oss;
        boost::archive::text_oarchive oa(oss);
        oa << transaction;

        return oss.str();
    }

    bool sign_transaction(const std::string &serialized_transaction,
                          std::array<unsigned char, crypto_sign_BYTES> &signature,
                          std::array<unsigned char, crypto_sign_SECRETKEYBYTES> &sk) {
        return crypto_sign_detached(
                   signature.data(),
                   nullptr,
                   reinterpret_cast<const unsigned char *>(serialized_transaction.data()),
                   serialized_transaction.size(),
                   sk.data()) == 0;
    }

    bool verify_transaction(const std::string &serialized_transaction,
                            std::array<unsigned char, crypto_sign_BYTES> &signature,
                            std::array<unsigned char, crypto_sign_PUBLICKEYBYTES> &pk) {
        return crypto_sign_verify_detached(
                   signature.data(),
                   reinterpret_cast<const unsigned char *>(serialized_transaction.data()),
                   serialized_transaction.size(),
                   pk.data()) == 0;
    }
}

#endif //TRADINGEXCHANGE_EVENT_TYPES_H
