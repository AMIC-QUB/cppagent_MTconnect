//
// Copyright Copyright 2009-2019, AMT – The Association For Manufacturing Technology (“AMT”)
// All rights reserved.
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//       http://www.apache.org/licenses/LICENSE-2.0
//
//    Unless required by applicable law or agreed to in writing, software
//    distributed under the License is distributed on an "AS IS" BASIS,
//    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//    See the License for the specific language governing permissions and
//    limitations under the License.
//

#pragma once
#include "change_observer.hpp"
#include "component.hpp"
#include "globals.hpp"

#include <dlib/threads.h>

#include <map>

#ifdef PASCAL
#undef PASCAL
#endif

namespace mtconnect
{
  class Adapter;

  class DataItem : public ChangeSignaler
  {
   public:
    // Enumeration for data item category
    enum ECategory
    {
      SAMPLE,
      EVENT,
      CONDITION
    };

    enum ERepresentation
    {
      VALUE,
      TIME_SERIES,
      DISCRETE,
      DATA_SET
    };

    enum EFilterType
    {
      FILTER_MINIMUM_DELTA,
      FILTER_PERIOD,
      FILTER_NONE
    };

   public:
    // Construct a data item with appropriate attributes mapping
    DataItem(std::map<std::string, std::string> const &attributes);

    // Destructor
    ~DataItem();

    // Get a map of all the attributes of this data item
    const std::map<std::string, std::string> &getAttributes() const
    {
      return m_attributes;
    }

    // Getter methods for data item specs
    const std::string &getId() const
    {
      return m_id;
    }
    const std::string &getName() const
    {
      return m_name;
    }
    const std::string &getSource() const
    {
      return m_source;
    }
    const std::string &getSourceDataItemId() const
    {
      return m_sourceDataItemId;
    }
    const std::string &getSourceComponentId() const
    {
      return m_sourceComponentId;
    }
    const std::string &getSourceCompositionId() const
    {
      return m_sourceCompositionId;
    }
    const std::string &getType() const
    {
      return m_type;
    }
    const std::string &getElementName() const
    {
      return m_camelType;
    }
    const std::string &getPrefixedElementName() const
    {
      return m_prefixedCamelType;
    }
    const std::string &getSubType() const
    {
      return m_subType;
    }
    const std::string &getNativeUnits() const
    {
      return m_nativeUnits;
    }
    const std::string &getUnits() const
    {
      return m_units;
    }
    const std::string &getPrefix() const
    {
      return m_prefix;
    }
    const std::string &getStatistic() const
    {
      return m_statistic;
    }
    const std::string &getSampleRate() const
    {
      return m_sampleRate;
    }
    const std::string &getCompositionId() const
    {
      return m_compositionId;
    }
    float getNativeScale() const
    {
      return m_nativeScale;
    }
    double getConversionFactor() const
    {
      return m_conversionFactor;
    }
    double getConversionOffset() const
    {
      return m_conversionOffset;
    }
    bool hasFactor() const
    {
      return m_hasFactor;
    }
    ECategory getCategory() const
    {
      return m_category;
    }
    ERepresentation getRepresentation() const
    {
      return m_representation;
    }

    void setConversionFactor(double factor, double offset);

    // Returns if data item has this attribute
    bool hasName(const std::string &name) const;
    bool hasNativeScale() const
    {
      return m_hasNativeScale;
    }

    // Add a source (extra information) to data item
    void addSource(const std::string &source, const std::string &sourceDataItemId,
                   const std::string &sourceComponentId, const std::string &sourceCompositionId)
    {
      m_source = source;
      m_sourceDataItemId = sourceDataItemId;
      m_sourceComponentId = sourceComponentId;
      m_sourceCompositionId = sourceCompositionId;
    }

    // Returns true if data item is a sample
    bool isSample() const
    {
      return m_category == SAMPLE;
    }
    bool isEvent() const
    {
      return m_category == EVENT;
    }
    bool isCondition() const
    {
      return m_category == CONDITION;
    }
    bool isAlarm() const
    {
      return m_isAlarm;
    }
    bool isMessage() const
    {
      return m_isMessage;
    }
    bool isAssetChanged() const
    {
      return m_isAssetChanged;
    }
    bool isAssetRemoved() const
    {
      return m_isAssetRemoved;
    }
    bool isTimeSeries() const
    {
      return m_representation == TIME_SERIES;
    }
    bool isDiscreteRep() const
    {
      return m_representation == DISCRETE;
    }
    bool isDataSet() const
    {
      return m_representation == DATA_SET;
    }
    bool isDiscrete() const
    {
      return m_isDiscrete;
    }
    bool allowDups() const
    {
      return isDiscrete() || isDiscreteRep();
    }
    bool is3D() const
    {
      return m_threeD;
    }

    bool hasResetTrigger() const
    {
      return !m_resetTrigger.empty();
    }
    const std::string &getResetTrigger() const
    {
      return m_resetTrigger;
    }
    void setResetTrigger(const std::string &aTrigger)
    {
      m_resetTrigger = aTrigger;
    }

    bool hasInitialValue() const
    {
      return !m_initialValue.empty();
    }
    const std::string &getInitialValue() const
    {
      return m_initialValue;
    }
    void setInitialValue(const std::string &aValue)
    {
      m_initialValue = aValue;
    }

    // Set/get component that data item is associated with
    void setComponent(Component &component)
    {
      m_component = &component;
    }
    Component *getComponent() const
    {
      return m_component;
    }

    // Get the name for the adapter feed
    const std::string &getSourceOrName() const
    {
      return m_source.empty() ? (m_name.empty() ? m_id : m_name) : m_source;
    }

    // Transform a name to camel casing
    static std::string getCamelType(const std::string &type, std::string &prefix);

    // Duplicate Checking
    bool isDuplicate(const std::string &value)
    {
      // Do not dup check for time series.
      if (m_representation != VALUE || m_isDiscrete)
        return false;
      else if (value == m_lastValue)
        return true;

      m_lastValue = value;
      return false;
    }

    // Filter checking
    bool isFiltered(const double value, const double timeOffset)
    {
      if (m_hasMinimumDelta && m_category == SAMPLE)
      {
        if (!ISNAN(m_lastSampleValue))
        {
          if (value > (m_lastSampleValue - m_filterValue) &&
              value < (m_lastSampleValue + m_filterValue))
          {
            // Filter value
            return true;
          }
        }

        m_lastSampleValue = value;
      }

      if (m_hasMinimumPeriod)
      {
        if (!ISNAN(m_lastTimeOffset) && !ISNAN(timeOffset))
        {
          if (timeOffset < (m_lastTimeOffset + m_filterPeriod))
          {
            // Filter value
            return true;
          }
        }

        m_lastTimeOffset = timeOffset;
      }

      return false;
    }

    // Constraints
    bool hasConstraints() const
    {
      return m_hasConstraints;
    }
    std::string getMaximum() const
    {
      return m_maximum;
    }
    std::string getMinimum() const
    {
      return m_minimum;
    }
    const std::vector<std::string> &getConstrainedValues() const
    {
      return m_values;
    }
    bool hasConstantValue() const
    {
      return m_values.size() == 1;
    }

    bool hasMinimumDelta() const
    {
      return m_hasMinimumDelta;
    }
    bool hasMinimumPeriod() const
    {
      return m_hasMinimumPeriod;
    }
    double getFilterValue() const
    {
      return m_filterValue;
    }
    double getFilterPeriod() const
    {
      return m_filterPeriod;
    }

    void setMaximum(std::string max)
    {
      m_maximum = max;
      m_hasConstraints = true;
    }
    void setMinimum(std::string min)
    {
      m_minimum = min;
      m_hasConstraints = true;
    }
    void addConstrainedValue(std::string value)
    {
      m_values.emplace_back(value);
      m_hasConstraints = true;
    }

    void setMinmumDelta(double value)
    {
      m_filterValue = value;
      m_hasMinimumDelta = true;
    }
    void setMinmumPeriod(double value)
    {
      m_filterPeriod = value;
      m_hasMinimumPeriod = true;
    }

    bool conversionRequired();
    std::string convertValue(const std::string &value);
    float convertValue(float value);

    Adapter *getDataSource() const
    {
      return m_dataSource;
    }
    void setDataSource(Adapter *source);
    bool operator<(const DataItem &another) const;

    bool operator==(DataItem &another) const
    {
      return m_id == another.m_id;
    }

    const char *getCategoryText() const
    {
      switch (m_category)
      {
        case DataItem::SAMPLE:
          return "Samples";

        case DataItem::EVENT:
          return "Events";

        case DataItem::CONDITION:
          return "Condition";

        default:
          return "Undefined";
      }
    }

   protected:
    double simpleFactor(const std::string &units);
    std::map<std::string, std::string> buildAttributes() const;
    void computeConversionFactors();

   protected:
    // Unique ID for each component
    std::string m_id;

    // Name for itself
    std::string m_name;

    // Type of data item
    std::string m_type;
    std::string m_camelType;
    std::string m_prefixedCamelType;
    std::string m_prefix;

    // Subtype of data item
    std::string m_subType;

    // Category of data item
    ECategory m_category;

    // Native units of data item
    std::string m_nativeUnits;

    // Units of data item
    std::string m_units;

    // The statistical process used on this data item
    std::string m_statistic;

    // Representation of data item
    ERepresentation m_representation;
    std::string m_sampleRate;

    std::string m_compositionId;

    // Native scale of data item
    float m_nativeScale;
    bool m_hasNativeScale;
    bool m_threeD;
    bool m_isMessage;
    bool m_isAlarm;
    bool m_isAssetChanged;
    bool m_isAssetRemoved;
    bool m_isDiscrete;

    // Sig figs of data item
    unsigned int m_significantDigits;
    bool m_hasSignificantDigits;

    // Coordinate system of data item
    std::string m_coordinateSystem;

    // Extra source information of data item
    std::string m_source;
    std::string m_sourceDataItemId;
    std::string m_sourceComponentId;
    std::string m_sourceCompositionId;

    // The reset trigger;
    std::string m_resetTrigger;

    // Initial value
    std::string m_initialValue;

    // Constraints for this data item
    std::string m_maximum;
    std::string m_minimum;
    std::vector<std::string> m_values;
    bool m_hasConstraints;

    double m_filterValue;
    // Period filter, in seconds
    double m_filterPeriod;
    bool m_hasMinimumDelta;
    bool m_hasMinimumPeriod;

    // Component that data item is associated with
    Component *m_component;

    // Duplicate and filter checking
    std::string m_lastValue;
    double m_lastSampleValue;
    double m_lastTimeOffset;

    // Attrubutes
    std::map<std::string, std::string> m_attributes;

    // The data source for this data item
    Adapter *m_dataSource;

    // Conversion factor
    double m_conversionFactor;
    double m_conversionOffset;
    bool m_conversionDetermined;
    bool m_conversionRequired;
    bool m_hasFactor;
  };
}  // namespace mtconnect
