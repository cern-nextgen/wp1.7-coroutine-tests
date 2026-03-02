#pragma once
#include <iostream>

template <typename Tag>
class StatusCodeImpl {
    public:
    enum class Status { SUCCESS = 0, FAILURE = 1, UNDEFINED = 2 };
    inline static const StatusCodeImpl SUCCESS{Status::SUCCESS};
    inline static const StatusCodeImpl FAILURE{Status::FAILURE};
    inline static const StatusCodeImpl UNDEFINED{Status::UNDEFINED};
    StatusCodeImpl(Status status = Status::UNDEFINED) : m_status(status) {}

    bool operator==(const StatusCodeImpl& other) const {
        return m_status == other.m_status;
    }
    Status status() const { return m_status; }

    private:
    Status m_status;
};

template <typename Tag>
std::ostream& operator<<(std::ostream& os, const StatusCodeImpl<Tag>& sc) {
    os << Tag::name << "::StatusCode::";
    switch (sc.status()) {
        case StatusCodeImpl<Tag>::Status::SUCCESS:
            os << "SUCCESS";
            break;
        case StatusCodeImpl<Tag>::Status::FAILURE:
            os << "FAILURE";
            break;
        case StatusCodeImpl<Tag>::Status::UNDEFINED:
            os << "UNDEFINED";
            break;
    }
    return os;
}
