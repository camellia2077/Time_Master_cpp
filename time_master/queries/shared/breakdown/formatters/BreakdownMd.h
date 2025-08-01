// queries/shared/breakdown/formatters/BreakdownMd.h 
#ifndef PROJECT_BREAKDOWN_MD_FORMAT_H
#define PROJECT_BREAKDOWN_MD_FORMAT_H

#include "queries/shared/breakdown/ITreeFmt.h"

class BreakdownMd : public ITreeFmt {
public:
    std::string format(const ProjectTree& tree, long long total_duration, int avg_days) const override;

private:
    // 递归辅助函数，用于生成层级的输出
    void generate_sorted_output(std::stringstream& ss, const ProjectNode& node, int indent, int avg_days) const;
};

#endif // PROJECT_BREAKDOWN_MD_FORMAT_H