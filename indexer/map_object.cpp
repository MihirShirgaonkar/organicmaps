#include "map_object.hpp"

#include "indexer/feature.hpp"
#include "indexer/feature_algo.hpp"
#include "indexer/feature_utils.hpp"
#include "indexer/ftypes_matcher.hpp"

#include "platform/localization.hpp"
#include "platform/measurement_utils.hpp"
#include "platform/preferred_languages.hpp"

#include "base/logging.hpp"
#include "base/string_utils.hpp"

using namespace std;

namespace osm
{
namespace
{
constexpr char const * kWlan = "wlan";
constexpr char const * kWired = "wired";
constexpr char const * kTerminal = "terminal";
constexpr char const * kYes = "yes";
constexpr char const * kNo = "no";
constexpr char const * kFieldsSeparator = " • ";
}  // namespace

string DebugPrint(osm::Internet internet)
{
  switch (internet)
  {
  case Internet::No: return kNo;
  case Internet::Yes: return kYes;
  case Internet::Wlan: return kWlan;
  case Internet::Wired: return kWired;
  case Internet::Terminal: return kTerminal;
  case Internet::Unknown: break;
  }
  return {};
}

string DebugPrint(Props props)
{
  string k;
  switch (props)
  {
  case osm::Props::Phone: k = "phone"; break;
  case osm::Props::Fax: k = "fax"; break;
  case osm::Props::Email: k = "email"; break;
  case osm::Props::Website: k = "website"; break;
  case osm::Props::ContactFacebook: k = "contact:facebook"; break;
  case osm::Props::ContactInstagram: k = "contact:instagram"; break;
  case osm::Props::ContactTwitter: k = "contact:twitter"; break;
  case osm::Props::ContactVk: k = "contact:vk"; break;
  case osm::Props::ContactLine: k = "contact:line"; break;
  case osm::Props::Internet: k = "internet_access"; break;
  case osm::Props::Cuisine: k = "cuisine"; break;
  case osm::Props::OpeningHours: k = "opening_hours"; break;
  case osm::Props::Stars: k = "stars"; break;
  case osm::Props::Operator: k = "operator"; break;
  case osm::Props::Elevation: k = "ele"; break;
  case osm::Props::Wikipedia: k = "wikipedia"; break;
  case osm::Props::Flats: k = "addr:flats"; break;
  case osm::Props::BuildingLevels: k = "building:levels"; break;
  case osm::Props::Level: k = "level"; break;
  }
  return k;
}

void MapObject::SetFromFeatureType(FeatureType & ft)
{
  m_mercator = feature::GetCenter(ft);
  m_name = ft.GetNames();

  Classificator const & cl = classif();
  m_types = feature::TypesHolder(ft);
  m_types.RemoveIf([&cl](uint32_t t)
  {
    return !cl.IsTypeValid(t);
  });
  // Actually, we can't select object on map with invalid (non-drawable type).
  ASSERT(!m_types.Empty(), ());

  m_metadata = ft.GetMetadata();
  m_houseNumber = ft.GetHouseNumber();
  m_roadNumber = ft.GetRoadNumber();
  m_featureID = ft.GetID();
  m_geomType = ft.GetGeomType();
  if (m_geomType == feature::GeomType::Area)
  {
    m_triangles = ft.GetTrianglesAsPoints(FeatureType::BEST_GEOMETRY);
  }
  else if (m_geomType == feature::GeomType::Line)
  {
    ft.ParseGeometry(FeatureType::BEST_GEOMETRY);
    m_points.reserve(ft.GetPointsCount());
    ft.ForEachPoint([this](m2::PointD const & p) { m_points.push_back(p); },
                    FeatureType::BEST_GEOMETRY);
  }

#ifdef DEBUG
  if (ftypes::IsWifiChecker::Instance()(ft))
    ASSERT(m_metadata.Has(feature::Metadata::FMD_INTERNET), ());
#endif
}

FeatureID const & MapObject::GetID() const { return m_featureID; }
ms::LatLon MapObject::GetLatLon() const { return mercator::ToLatLon(m_mercator); }
m2::PointD const & MapObject::GetMercator() const { return m_mercator; }
vector<m2::PointD> const & MapObject::GetTriangesAsPoints() const { return m_triangles; }
vector<m2::PointD> const & MapObject::GetPoints() const { return m_points; }
feature::TypesHolder const & MapObject::GetTypes() const { return m_types; }

string MapObject::GetDefaultName() const
{
  string_view name;
  UNUSED_VALUE(m_name.GetString(StringUtf8Multilang::kDefaultCode, name));

  /// @todo Can return string_view?
  return std::string(name);
}

StringUtf8Multilang const & MapObject::GetNameMultilang() const
{
  return m_name;
}

string const & MapObject::GetHouseNumber() const { return m_houseNumber; }

string MapObject::GetLocalizedType() const
{
  ASSERT(!m_types.Empty(), ());
  feature::TypesHolder copy(m_types);
  copy.SortBySpec();

  return platform::GetLocalizedTypeName(classif().GetReadableObjectName(*copy.begin()));
}

vector<osm::Props> MapObject::AvailableProperties() const
{
  auto props = MetadataToProps(m_metadata.GetPresentTypes());

  auto const & isCuisine = ftypes::IsCuisineChecker::Instance();
  if (any_of(m_types.begin(), m_types.end(), [&](auto const t) { return isCuisine(t); }))
  {
    props.push_back(Props::Cuisine);
    base::SortUnique(props);
  }

  return props;
}

string_view MapObject::GetPhone() const { return m_metadata.Get(feature::Metadata::FMD_PHONE_NUMBER); }
string_view MapObject::GetFax() const { return m_metadata.Get(feature::Metadata::FMD_FAX_NUMBER); }
string_view MapObject::GetEmail() const { return m_metadata.Get(feature::Metadata::FMD_EMAIL); }

string_view MapObject::GetWebsite() const
{
  auto website = m_metadata.Get(feature::Metadata::FMD_WEBSITE);
  if (website.empty())
    website = m_metadata.Get(feature::Metadata::FMD_URL);
  return website;
}

string_view MapObject::GetFacebookPage() const
{
  return m_metadata.Get(feature::Metadata::FMD_CONTACT_FACEBOOK);
}

string_view MapObject::GetInstagramPage() const
{
  return m_metadata.Get(feature::Metadata::FMD_CONTACT_INSTAGRAM);
}

string_view MapObject::GetTwitterPage() const
{
  return m_metadata.Get(feature::Metadata::FMD_CONTACT_TWITTER);
}

string_view MapObject::GetVkPage() const
{
  return m_metadata.Get(feature::Metadata::FMD_CONTACT_VK);
}

string_view MapObject::GetLinePage() const
{
  return m_metadata.Get(feature::Metadata::FMD_CONTACT_LINE);
}

Internet MapObject::GetInternet() const
{
  return InternetFromString(m_metadata.Get(feature::Metadata::FMD_INTERNET));
}

Internet InternetFromString(std::string_view inet)
{
  if (inet.empty())
    return Internet::Unknown;
  if (inet.find(kWlan) != string::npos)
    return Internet::Wlan;
  if (inet.find(kWired) != string::npos)
    return Internet::Wired;
  if (inet.find(kTerminal) != string::npos)
    return Internet::Terminal;
  if (inet == kYes)
    return Internet::Yes;
  if (inet == kNo)
    return Internet::No;
  return Internet::Unknown;
}

vector<string> MapObject::GetCuisines() const { return feature::GetCuisines(m_types); }

vector<string> MapObject::GetLocalizedCuisines() const
{
  return feature::GetLocalizedCuisines(m_types);
}

vector<string> MapObject::GetRecyclingTypes() const { return feature::GetRecyclingTypes(m_types); }

vector<string> MapObject::GetLocalizedRecyclingTypes() const
{
  return feature::GetLocalizedRecyclingTypes(m_types);
}

string MapObject::FormatCuisines() const
{
  return strings::JoinStrings(GetLocalizedCuisines(), kFieldsSeparator);
}

vector<string> MapObject::GetRoadShields() const
{
  return feature::GetRoadShieldsNames(m_roadNumber);
}

string MapObject::FormatRoadShields() const
{
  return strings::JoinStrings(GetRoadShields(), kFieldsSeparator);
}

string_view MapObject::GetOpeningHours() const
{
  return m_metadata.Get(feature::Metadata::FMD_OPEN_HOURS);
}

string_view MapObject::GetOperator() const { return m_metadata.Get(feature::Metadata::FMD_OPERATOR); }

int MapObject::GetStars() const
{
  uint8_t count = 0;

  auto const sv = m_metadata.Get(feature::Metadata::FMD_STARS);
  if (!sv.empty())
  {
    if (!strings::to_uint(sv, count))
      count = 0;
  }

  return count;
}

string MapObject::GetElevationFormatted() const
{
  if (m_metadata.Has(feature::Metadata::FMD_ELE))
  {
    double value;
    if (strings::to_double(m_metadata.Get(feature::Metadata::FMD_ELE), value))
      return measurement_utils::FormatAltitude(value);
    else
      LOG(LWARNING,
          ("Invalid metadata for elevation:", m_metadata.Get(feature::Metadata::FMD_ELE)));
  }
  return {};
}

bool MapObject::GetElevation(double & outElevationInMeters) const
{
  return strings::to_double(m_metadata.Get(feature::Metadata::FMD_ELE), outElevationInMeters);
}

string MapObject::GetWikipediaLink() const { return m_metadata.GetWikiURL(); }

string MapObject::GetWikimediaCommonsLink() const { return m_metadata.GetWikimediaCommonsURL(); }

string_view MapObject::GetFlats() const { return m_metadata.Get(feature::Metadata::FMD_FLATS); }

string_view MapObject::GetLevel() const { return m_metadata.Get(feature::Metadata::FMD_LEVEL); }

string_view MapObject::GetBuildingLevels() const
{
  return m_metadata.Get(feature::Metadata::FMD_BUILDING_LEVELS);
}

ftraits::WheelchairAvailability MapObject::GetWheelchairType() const
{
  auto const opt = ftraits::Wheelchair::GetValue(m_types);
  return opt ? *opt : ftraits::WheelchairAvailability::No;
}

string_view MapObject::GetAirportIata() const
{
  return m_metadata.Get(feature::Metadata::FMD_AIRPORT_IATA);
}

feature::Metadata const & MapObject::GetMetadata() const { return m_metadata; }
bool MapObject::IsPointType() const { return m_geomType == feature::GeomType::Point; }
bool MapObject::IsBuilding() const { return ftypes::IsBuildingChecker::Instance()(m_types); }

}  // namespace osm
