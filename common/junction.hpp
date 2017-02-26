#pragma once
#include "uuid.hpp"
#include "common.hpp"
#include "object.hpp"
#include "uuid_provider.hpp"
#include "net.hpp"
#include "uuid_ptr.hpp"
#include "json.hpp"
#include "bus.hpp"
#include <vector>
#include <map>
#include <fstream>
		

namespace horizon {
	using json = nlohmann::json;
	
	/**
	 * A Junction is a point in 2D-Space.
	 * A Junction is referenced by Line, Arc, LineNet, etc.\ for storing coordinates.
	 * This allows for actually storing Line connections instead of relying
	 * on coincident coordinates.
	 * When used on a Board or a Sheet, a Junction may get assigned a Net
	 * or a Bus and a net segment.
	 */
	class Junction: public UUIDProvider {
		public :
			Junction(const UUID &uu, const json &j);
			Junction(const UUID &uu);

			UUID uuid;
			Coord<int64_t> position;


			virtual UUID get_uuid() const ;
			
			//not stored
			uuid_ptr<Net> net=nullptr;
			uuid_ptr<Bus> bus = nullptr;
			UUID net_segment = UUID();
			bool temp;
			bool warning = false;
			int layer = 10000;
			bool needs_via = false;
			bool has_via = false;
			unsigned int connection_count = 0;

			json serialize() const;
	};
}
