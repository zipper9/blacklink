#ifndef DCPLUSPLUS_CLIENT_FLAGS_H_
#define DCPLUSPLUS_CLIENT_FLAGS_H_

#include <stdint.h>

template<typename T>
class BaseFlags
{
	public:
		typedef T MaskType;

		BaseFlags() : flags(0) {}
		explicit BaseFlags(MaskType f) : flags(f) {}

		bool isSet(MaskType flag) const
		{
			return (flags & flag) == flag;
		}

		bool isAnySet(MaskType flag) const
		{
			return (flags & flag) != 0;
		}

		void setFlag(MaskType flag)
		{
			flags |= flag;
		}

		void unsetFlag(MaskType flag)
		{
			flags &= ~flag;
		}

		void setFlag(MaskType flag, bool value)
		{
			if (value)
				flags |= flag;
			else
				flags &= ~flag;
		}

		void setFlags(MaskType newFlags)
		{
			flags = newFlags;
		}

		MaskType getFlags() const
		{
			return flags;
		}

	private:
		MaskType flags;
};

typedef BaseFlags<uint32_t> Flags;

#endif /* DCPLUSPLUS_CLIENT_FLAGS_H_ */
