#include "cvars.h"

#include <unordered_map>

#include <array>
#include <algorithm>
#include "imgui.h"
#include "imgui_stdlib.h"
#include "imgui_internal.h"
#include <shared_mutex>

enum class CVarType : char
{
	INT,
	FLOAT,
	STRING
};

class CVarParameter
{
public:
	friend class CVarSystemImpl;

	int32_t arrayIndex;

	CVarType type;
	CVarFlags flags;
	std::string name;
	std::string description;
};

template <typename T>
struct CVarStorage
{
	T initial;
	T current;
	CVarParameter *parameter;
};

template <typename T>
struct CVarArray
{
	CVarStorage<T> *cvars{nullptr};
	int32_t lastCVar{0};

	CVarArray(size_t size)
	{
		cvars = new CVarStorage<T>[size]();
	}

	CVarStorage<T> *GetCurrentStorage(int32_t index)
	{
		return &cvars[index];
	}

	T *GetCurrentPtr(int32_t index)
	{
		return &cvars[index].current;
	}

	T GetCurrent(int32_t index)
	{
		return cvars[index].current;
	}

	void SetCurrent(const T &val, int32_t index)
	{
		cvars[index].current = val;
	}

	int Add(const T &value, CVarParameter *param)
	{
		int index = lastCVar;
		cvars[index].current = value;
		cvars[index].initial = value;
		cvars[index].parameter = param;

		param->arrayIndex = index;
		lastCVar++;
		return index;
	}

	int Add(const T &initialValue, const T &currentValue, CVarParameter *param)
	{
		int index = lastCVar;

		cvars[index].current = currentValue;
		cvars[index].initial = initialValue;
		cvars[index].parameter = param;

		param->arrayIndex = index;
		lastCVar++;

		return index;
	}
};

uint32_t Hash(const char *str)
{
	return StringUtils::Fnv1a32(str, strlen(str));
}

class CVarSystemImpl : public CVarSystem
{
public:
	CVarParameter *GetCVar(StringUtils::StringHash hash) override final;

	CVarParameter *CreateFloatCVar(const char *name, const char *description, double defaultValue, double currentValue) override final;

	CVarParameter *CreateIntCVar(const char *name, const char *description, int32_t defaultValue, int32_t currentValue) override final;

	CVarParameter *CreateStringCVar(const char *name, const char *description, const char *defaultValue, const char *currentValue) override final;

	double *GetFloatCVar(StringUtils::StringHash hash) override final;
	int32_t *GetIntCVar(StringUtils::StringHash hash) override final;
	const char *GetStringCVar(StringUtils::StringHash hash) override final;

	void SetFloatCVar(StringUtils::StringHash hash, double value) override final;

	void SetIntCVar(StringUtils::StringHash hash, int32_t value) override final;

	void SetStringCVar(StringUtils::StringHash hash, const char *value) override final;

	void DrawImguiEditor() override final;

	void EditParameter(CVarParameter *param, float textWidth);

	constexpr static int MAX_INT_CVARS = 1000;
	CVarArray<int32_t> intCVars{MAX_INT_CVARS};

	constexpr static int MAX_FLOAT_CVARS = 1000;
	CVarArray<double> floatCVars{MAX_FLOAT_CVARS};

	constexpr static int MAX_STRING_CVARS = 1000;
	CVarArray<std::string> stringCVars{MAX_STRING_CVARS};

	// using template with specialization to get the cvar array for each type
	template <typename T>
	CVarArray<T> *GetCVarArray();

	template <>
	CVarArray<int32_t> *GetCVarArray()
	{
		return &intCVars;
	}

	template <>
	CVarArray<double> *GetCVarArray()
	{
		return &floatCVars;
	}
	template <>
	CVarArray<std::string> *GetCVarArray()
	{
		return &stringCVars;
	}

	// templated get-set cvar versions for syntax sugar
	template <typename T>
	T *GetCVarCurrent(uint32_t nameHash)
	{
		CVarParameter *par = GetCVar(nameHash);
		if (!par)
		{
			return nullptr;
		}
		else
		{
			return GetCVarArray<T>()->GetCurrentPtr(par->arrayIndex);
		}
	}

	template <typename T>
	void SetCVarCurrent(uint32_t nameHash, const T &value)
	{
		CVarParameter *cvar = GetCVar(nameHash);
		if (cvar)
		{
			GetCVarArray<T>()->SetCurrent(value, cvar->arrayIndex);
		}
	}

	static CVarSystemImpl *Get()
	{
		return static_cast<CVarSystemImpl *>(CVarSystem::Get());
	}

private:
	std::shared_mutex mutex_;

	CVarParameter *InitCVar(const char *name, const char *description);

	std::unordered_map<uint32_t, CVarParameter> saved_cvars_;

	std::vector<CVarParameter *> cached_edit_parameters_;
};

double *CVarSystemImpl::GetFloatCVar(StringUtils::StringHash hash)
{
	return GetCVarCurrent<double>(hash);
}

int32_t *CVarSystemImpl::GetIntCVar(StringUtils::StringHash hash)
{
	return GetCVarCurrent<int32_t>(hash);
}

const char *CVarSystemImpl::GetStringCVar(StringUtils::StringHash hash)
{
	return GetCVarCurrent<std::string>(hash)->c_str();
}

CVarSystem *CVarSystem::Get()
{
	static CVarSystemImpl cvarSys{};
	return &cvarSys;
}

CVarParameter *CVarSystemImpl::GetCVar(StringUtils::StringHash hash)
{
	std::shared_lock lock(mutex_);

	auto it = saved_cvars_.find(hash);

	if (it != saved_cvars_.end())
	{
		return &it->second;
	}

	return nullptr;
}

void CVarSystemImpl::SetFloatCVar(StringUtils::StringHash hash, double value)
{
	SetCVarCurrent<double>(hash, value);
}

void CVarSystemImpl::SetIntCVar(StringUtils::StringHash hash, int32_t value)
{
	SetCVarCurrent<int32_t>(hash, value);
}

void CVarSystemImpl::SetStringCVar(StringUtils::StringHash hash, const char *value)
{
	SetCVarCurrent<std::string>(hash, value);
}

CVarParameter *CVarSystemImpl::CreateFloatCVar(const char *name, const char *description, double defaultValue, double currentValue)
{
	std::unique_lock lock(mutex_);
	CVarParameter *param = InitCVar(name, description);
	if (!param)
	{
		return nullptr;
	}

	param->type = CVarType::FLOAT;

	GetCVarArray<double>()->Add(defaultValue, currentValue, param);

	return param;
}

CVarParameter *CVarSystemImpl::CreateIntCVar(const char *name, const char *description, int32_t defaultValue, int32_t currentValue)
{
	std::unique_lock lock(mutex_);
	CVarParameter *param = InitCVar(name, description);
	if (!param)
	{
		return nullptr;
	}

	param->type = CVarType::INT;

	GetCVarArray<int32_t>()->Add(defaultValue, currentValue, param);

	return param;
}

CVarParameter *CVarSystemImpl::CreateStringCVar(const char *name, const char *description, const char *defaultValue, const char *currentValue)
{
	std::unique_lock lock(mutex_);
	CVarParameter *param = InitCVar(name, description);
	if (!param)
	{
		return nullptr;
	}

	param->type = CVarType::STRING;

	GetCVarArray<std::string>()->Add(defaultValue, currentValue, param);

	return param;
}

CVarParameter *CVarSystemImpl::InitCVar(const char *name, const char *description)
{
	uint32_t nameHash = StringUtils::StringHash{name};

	saved_cvars_[nameHash] = CVarParameter{};

	CVarParameter &param = saved_cvars_[nameHash];

	param.name = name;
	param.description = description;

	return &param;
}

AutoCVar_Float::AutoCVar_Float(const char *name, const char *description, double defaultValue, CVarFlags flags)
{
	CVarParameter *cvar = CVarSystem::Get()->CreateFloatCVar(name, description, defaultValue, defaultValue);
	cvar->flags = flags;
	index = cvar->arrayIndex;
}

template <typename T>
T GetCVarCurrentByIndex(int32_t index)
{
	return CVarSystemImpl::Get()->GetCVarArray<T>()->GetCurrent(index);
}

template <typename T>
T *PtrGetCVarCurrentByIndex(int32_t index)
{
	return CVarSystemImpl::Get()->GetCVarArray<T>()->GetCurrentPtr(index);
}

template <typename T>
void SetCVarCurrentByIndex(int32_t index, const T &data)
{
	CVarSystemImpl::Get()->GetCVarArray<T>()->SetCurrent(data, index);
}

double AutoCVar_Float::Get()
{
	return GetCVarCurrentByIndex<CVarType>(index);
}

double *AutoCVar_Float::GetPtr()
{
	return PtrGetCVarCurrentByIndex<CVarType>(index);
}

float AutoCVar_Float::GetFloat()
{
	return static_cast<float>(Get());
}

float *AutoCVar_Float::GetFloatPtr()
{
	float *result = reinterpret_cast<float *>(GetPtr());
	return result;
}

void AutoCVar_Float::Set(double f)
{
	SetCVarCurrentByIndex<CVarType>(index, f);
}

AutoCVar_Int::AutoCVar_Int(const char *name, const char *description, int32_t defaultValue, CVarFlags flags)
{
	CVarParameter *cvar = CVarSystem::Get()->CreateIntCVar(name, description, defaultValue, defaultValue);
	cvar->flags = flags;
	index = cvar->arrayIndex;
}

int32_t AutoCVar_Int::Get()
{
	return GetCVarCurrentByIndex<CVarType>(index);
}

int32_t *AutoCVar_Int::GetPtr()
{
	return PtrGetCVarCurrentByIndex<CVarType>(index);
}

void AutoCVar_Int::Set(int32_t val)
{
	SetCVarCurrentByIndex<CVarType>(index, val);
}

void AutoCVar_Int::Toggle()
{
	bool enabled = Get() != 0;

	Set(enabled ? 0 : 1);
}

AutoCVar_String::AutoCVar_String(const char *name, const char *description, const char *defaultValue, CVarFlags flags)
{
	CVarParameter *cvar = CVarSystem::Get()->CreateStringCVar(name, description, defaultValue, defaultValue);
	cvar->flags = flags;
	index = cvar->arrayIndex;
}

const char *AutoCVar_String::Get()
{
	return GetCVarCurrentByIndex<CVarType>(index).c_str();
};

void AutoCVar_String::Set(std::string &&val)
{
	SetCVarCurrentByIndex<CVarType>(index, val);
}

void CVarSystemImpl::DrawImguiEditor()
{
	static std::string searchText = "";

	ImGui::InputText("Filter", &searchText);
	static bool bShowAdvanced = false;
	ImGui::Checkbox("Advanced", &bShowAdvanced);
	ImGui::Separator();
	cached_edit_parameters_.clear();

	auto add_to_edit_list = [&](auto param)
	{
		bool bHidden = ((uint32_t)param->flags & (uint32_t)CVarFlags::Noedit);
		bool bIsAdvanced = ((uint32_t)param->flags & (uint32_t)CVarFlags::Advanced);

		if (!bHidden)
		{
			if (!(!bShowAdvanced && bIsAdvanced) && param->name.find(searchText) != std::string::npos)
			{
				cached_edit_parameters_.push_back(param);
			}
		}
	};

	for (int i = 0; i < GetCVarArray<int32_t>()->lastCVar; i++)
	{
		add_to_edit_list(GetCVarArray<int32_t>()->cvars[i].parameter);
	}

	for (int i = 0; i < GetCVarArray<double>()->lastCVar; i++)
	{
		add_to_edit_list(GetCVarArray<double>()->cvars[i].parameter);
	}

	for (int i = 0; i < GetCVarArray<std::string>()->lastCVar; i++)
	{
		add_to_edit_list(GetCVarArray<std::string>()->cvars[i].parameter);
	}

	if (cached_edit_parameters_.size() > 10)
	{
		std::unordered_map<std::string, std::vector<CVarParameter *>> categorizedParams;

		// insert all the edit parameters into the hashmap by category
		for (auto p : cached_edit_parameters_)
		{
			int dotPos = -1;
			for (int i = 0; i < p->name.length(); i++)
			{
				if (p->name[i] == '.')
				{
					dotPos = i;
					break;
				}
			}

			std::string catergory = "";
			if (dotPos != -1)
			{
				catergory = p->name.substr(0, dotPos);
			}

			auto it = categorizedParams.find(catergory);
			if (it == categorizedParams.end())
			{
				categorizedParams[catergory] = std::vector<CVarParameter *>();
				it = categorizedParams.find(catergory);
			}
			it->second.push_back(p);
		}

		for (auto [category, parameters] : categorizedParams)
		{
			// alphabetical sort
			std::sort(parameters.begin(), parameters.end(), [](CVarParameter *a, CVarParameter *b)
					  { return a->name < b->name; });

			if (ImGui::BeginMenu(category.c_str()))
			{
				float maxTextWidth = 0;

				for (auto p : parameters)
				{
					maxTextWidth = std::max(maxTextWidth, ImGui::CalcTextSize(p->name.c_str()).x);
				}

				for (auto p : parameters)
				{
					EditParameter(p, maxTextWidth);
				}

				ImGui::EndMenu();
			}
		}
	}
	else
	{
		// alphabetical sort
		std::sort(cached_edit_parameters_.begin(), cached_edit_parameters_.end(), [](CVarParameter *a, CVarParameter *b)
				  { return a->name < b->name; });

		float maxTextWidth = 0;

		for (auto p : cached_edit_parameters_)
		{
			maxTextWidth = std::max(maxTextWidth, ImGui::CalcTextSize(p->name.c_str()).x);
		}

		for (auto p : cached_edit_parameters_)
		{
			EditParameter(p, maxTextWidth);
		}
	}
}

void Label(const char *label, float textWidth)
{
	constexpr float Slack = 50;
	constexpr float EditorWidth = 100;

	ImGuiWindow *window = ImGui::GetCurrentWindow();
	const ImVec2 lineStart = ImGui::GetCursorScreenPos();
	const ImGuiStyle &style = ImGui::GetStyle();
	float fullWidth = textWidth + Slack;

	ImVec2 textSize = ImGui::CalcTextSize(label);

	ImVec2 startPos = ImGui::GetCursorScreenPos();

	ImGui::Text(label);

	ImVec2 finalPos = {startPos.x + fullWidth, startPos.y};

	ImGui::SameLine();
	ImGui::SetCursorScreenPos(finalPos);

	ImGui::SetNextItemWidth(EditorWidth);
}

void CVarSystemImpl::EditParameter(CVarParameter *p, float textWidth)
{
	const bool readonlyFlag = ((uint32_t)p->flags & (uint32_t)CVarFlags::EditReadOnly);
	const bool checkboxFlag = ((uint32_t)p->flags & (uint32_t)CVarFlags::EditCheckBox);
	const bool dragFlag = ((uint32_t)p->flags & (uint32_t)CVarFlags::EditFloatDrag);

	switch (p->type)
	{
	case CVarType::INT:
	{
		if (readonlyFlag)
		{
			std::string displayFormat = p->name + "= %i";
			ImGui::Text(displayFormat.c_str(), GetCVarArray<int32_t>()->GetCurrent(p->arrayIndex));
		}
		else
		{
			if (checkboxFlag)
			{
				bool bCheckBox = GetCVarArray<int32_t>()->GetCurrent(p->arrayIndex) != 0;
				Label(p->name.c_str(), textWidth);

				ImGui::PushID(p->name.c_str());

				if (ImGui::Checkbox("", &bCheckBox))
				{
					GetCVarArray<int32_t>()->SetCurrent(bCheckBox ? 1 : 0, p->arrayIndex);
				}
				ImGui::PopID();
			}
			else
			{
				Label(p->name.c_str(), textWidth);
				ImGui::PushID(p->name.c_str());
				ImGui::InputInt("", GetCVarArray<int32_t>()->GetCurrentPtr(p->arrayIndex));
				ImGui::PopID();
			}
		}
		break;
	}
	case CVarType::FLOAT:
	{
		if (readonlyFlag)
		{
			std::string displayFormat = p->name + "= %f";
			ImGui::Text(displayFormat.c_str(), GetCVarArray<double>()->GetCurrent(p->arrayIndex));
		}
		else
		{
			Label(p->name.c_str(), textWidth);
			ImGui::PushID(p->name.c_str());
			if (dragFlag)
			{
				ImGui::InputDouble("", GetCVarArray<double>()->GetCurrentPtr(p->arrayIndex), 0, 0, "%.3f");
			}
			else
			{
				ImGui::InputDouble("", GetCVarArray<double>()->GetCurrentPtr(p->arrayIndex), 0, 0, "%.3f");
			}
			ImGui::PopID();
		}
		break;
	}
	case CVarType::STRING:
	{
		if (readonlyFlag)
		{
			std::string displayFormat = p->name + "= %s";
			ImGui::PushID(p->name.c_str());
			ImGui::Text(displayFormat.c_str(), GetCVarArray<std::string>()->GetCurrent(p->arrayIndex));

			ImGui::PopID();
		}
		else
		{
			Label(p->name.c_str(), textWidth);
			ImGui::InputText("", GetCVarArray<std::string>()->GetCurrentPtr(p->arrayIndex));

			ImGui::PopID();
		}
		break;
	}
	default:
		break;
	}

	if (ImGui::IsItemHovered)
	{
		ImGui::SetTooltip(p->description.c_str());
	}
}