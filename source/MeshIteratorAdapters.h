#pragma once

#include "MeshEnums.h"

namespace fea_io
{
	/// The MeshWriter allows you to use this interface to write nodes instead of allocating vectors of data
	/// This is more efficient if you have a custom node class in some container.
	/// If the container is not STL, then you will need to define an iterator inheriting from the std::iterator
	/// Create an interface what whatever node class you may have, and overload all of the methods
	template<typename GlobalIndex, typename LocalIndex, typename CoordType, typename InputIterator>
	class NodeIteratorAdapter {
	public:
		NodeIteratorAdapter() {};
		NodeIteratorAdapter(const InputIterator &it) {
			wrapped_iter_ = it;
		}

		// Implement these methods for you node type
		virtual const GlobalIndex get_global_id() = 0;
		virtual const LocalIndex get_parent_subdomain_index() = 0;
		virtual const CoordType* get_coords() = 0;
		virtual void set_global_id(const GlobalIndex) = 0;
		virtual void set_parent_subdomain_index(const LocalIndex) = 0;
		virtual void set_coords(const CoordType*) = 0;

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
	protected:
		InputIterator wrapped_iter_;
	};

	/// The MeshWriter allows you to use this interface to write nodes instead of allocating vectors of data
	/// This is more efficient if you have a custom node class in some container.
	/// If the container is not STL, then you will need to define an iterator inheriting from the std::iterator
	/// Create an interface what whatever node class you may have, and overload all of the methods
	template<typename GlobalIndex, typename LocalIndex, typename CoordType, typename InputIterator>
	class ElementIteratorAdapter {
	public:
		ElementIteratorAdapter() {};
		ElementIteratorAdapter(const InputIterator &it) {
			wrapped_iter_ = it;
		}

		// Implement these methods for you element type
		virtual const GlobalIndex get_global_id() = 0;
		virtual const ElementType get_geom_type() = 0;
		virtual const LocalIndex get_parent_subdomain_index() = 0;
		virtual const LocalIndex * get_local_connectivity() = 0;
		virtual void set_global_id(const GlobalIndex) = 0;
		virtual void set_geom_type(const ElementType) = 0;
		virtual void set_parent_subdomain_index(const LocalIndex) = 0;
		virtual void set_local_connectivity(const LocalIndex *) = 0;
		// Note: connectivity defined by local partition node numbering (not global)

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
	protected:
		InputIterator wrapped_iter_;
	};
}