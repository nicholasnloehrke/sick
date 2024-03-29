#pragma once

#include <deque>

#include "common/include/common/filter_base.h"
#include "common/include/common/filter_parameter.h"

#include "opencv2/core/mat.hpp"

namespace filter
{
	class moving_average_filter : public filter_base
	{
	public:
		moving_average_filter();
		~moving_average_filter() override;

		std::unique_ptr<filter_base> clone() const override;
		const std::string type() const override { return "moving-average-filter"; };
		const bool apply(cv::Mat& mat) const override;
		const bool load_json(const nlohmann::json& filter) override;
		const nlohmann::json to_json() const override;

	private:
		filter::filter_parameter<int, 2, 100> buffer_size;
		mutable std::deque<cv::Mat> buffer;
	};
}
