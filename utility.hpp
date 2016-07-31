#ifndef DATA_STRUCTURE_HPP
#define DATA_STRUCTURE_HPP

#include <QString>

struct update_info
{
    QString url_;
    QString version_;
    QString unzip_;
    QString copy_to_;
    QString display_name_;
};

template<typename T>
class guard_delete_later
{
public:
    guard_delete_later(guard_delete_later const&) = delete;
    guard_delete_later& operator=(guard_delete_later const&) = delete;
    guard_delete_later(guard_delete_later&&) = delete;
    guard_delete_later& operator=(guard_delete_later&&) = delete;

    explicit guard_delete_later(T *ptr) :
        ptr_(ptr)
    {}

    ~guard_delete_later()
    {
        ptr_->deleteLater();
    }

private:
    T *ptr_;
};

#endif // DATA_STRUCTURE_HPP
