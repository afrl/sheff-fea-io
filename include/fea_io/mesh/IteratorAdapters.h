#pragma once

#include "fea_io/common/Exceptions.h"

#include "MeshStructs.h"

namespace fea_io
{
	/// The MeshWriter allows you to use this interface to write nodes instead of allocating vectors of data
	/// This is far more efficient if you have a custom node class in some container.
	template<typename InputIterator, typename GlobalIndex,
		typename LocalIndex, typename CoordType>
	class NodeIteratorAdapter {
	public:
		NodeIteratorAdapter() {};
		NodeIteratorAdapter(const InputIterator &it) {
			wrapped_iter_ = it;
		}
		NodeIteratorAdapter& operator++ () {
			wrapped_iter_++;
			return *this;
		}
		bool operator==(NodeIteratorAdapter const & other) const {
			return wrapped_iter_ == other.wrapped_iter_;
		}
		bool operator!=(NodeIteratorAdapter const & other) const {
			return wrapped_iter_ != other.wrapped_iter_;
		}

		// Methods that require override
		virtual GlobalIndex get_global_number() = 0;
		virtual LocalIndex get_parent_partition() = 0;
		virtual LocalIndex get_coord_length() = 0;
		virtual const CoordType* get_coord() = 0;

	protected:
		InputIterator wrapped_iter_;
	};

	template<typename InputIterator, typename GlobalIndex,
		typename LocalIndex, typename CoordType>
	class ElementIteratorAdapter {
	public:
		ElementIteratorAdapter() {};
		ElementIteratorAdapter(const InputIterator &it) {
			wrapped_iter_ = it;
		}
		ElementIteratorAdapter& operator++ () {
			wrapped_iter_++;
			return *this;
		}
		bool operator==(ElementIteratorAdapter const & other) const {
			return wrapped_iter_ == other.wrapped_iter_;
		}
		bool operator!=(ElementIteratorAdapter const & other) const {
			return wrapped_iter_ != other.wrapped_iter_;
		}

		// Methods that require override
		virtual GlobalIndex get_global_number() = 0;
		virtual LocalIndex get_owning_partition() = 0;
		virtual LocalIndex get_component_id() = 0;
		virtual ElementType get_type() = 0;
		virtual LocalIndex get_local_conn_length() = 0;
		// Connectivity defined by local partition node numbering (not global)
		virtual const LocalIndex * get_local_conn() = 0;

	protected:
		InputIterator wrapped_iter_;
	};
}