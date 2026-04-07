# Pergyra Design Patterns

This document describes the official design patterns for Pergyra. These patterns represent repeatable, idiomatic structures for solving common problems using the language's unique type system (`subject`, `class`, `object`, `tobject`, `zone`, `intent`).

---

## 1. Notebook-style Analysis Pattern

**Description:**
A pattern for exploratory analysis where the goal is to answer a specific question by transforming raw data into a final report. It mimics the structure of a Jupyter Notebook but with strong typing and explicit state management.

**Structure:**
*   **Intent:** The analysis question (e.g., `ExploreOutliers`).
*   **Subject:** The raw input data (e.g., `RawDataset`).
*   **Zone:** The analysis session context (e.g., `AnalysisSession`).
*   **Objects:** Intermediate observation views (e.g., `SchemaView`, `MissingStats`).
*   **TObject:** The final exportable report (e.g., `EdaReport`).
*   **Class:** Calculation tools (e.g., `Profiler`).

**Example:**
See `examples/notebook_style_analysis.pgy`.

---

## 2. EDA (Exploratory Data Analysis) Pattern

**Description:**
A specialized variation of the Notebook pattern focused heavily on *intermediate observation objects*. EDA is about generating multiple views of the data (distributions, outliers, correlations) to understand it before making a final decision.

**Key Characteristic:** **Object-Heavy**. The value is in the `object` types created during the process.

**Structure:**
```text
intent ExploreData(zone: AnalysisSession, data: RawDataset) {
    // Step 1: Inspect Structure -> SchemaView
    step InspectSchema { ... }

    // Step 2: Check Data Quality -> MissingStats
    step AnalyzeMissing { ... }

    // Step 3: Calculate Statistics -> DistributionView
    step CalculateStats { ... }

    // Step 4: Find Anomalies -> OutlierView
    step DetectOutliers { ... }

    // Step 5: Generate Report -> EdaReport
    step GenerateReport { ... }
}
```

**Example:**
See `examples/eda_workflow.pgy`.

---

## 3. ETL (Extract, Transform, Load) Pattern

**Description:**
A pipeline-driven pattern for moving data from a source to a destination. Unlike EDA, which is exploratory, ETL is procedural and strict. It focuses on the flow of data through stages.

**Key Characteristic:** **Intent-Heavy**. The value is in the strict ordering and error handling of the `intent` steps.

**Structure:**
```text
intent RunCustomerEtl(zone: PipelineZone, batch: SourceBatch, extractor: Extractor) {
    // Step 1: Extract -> ExtractedRows
    step Extract { ... }

    // Step 2: Clean -> CleanedRows
    step Clean { ... }

    // Step 3: Transform -> TransformedRows
    step Transform { ... }

    // Step 4: Load -> LoadPayload
    step Load { ... }
}
```

**Roles:**
*   **Intent:** The pipeline controller.
*   **Subject:** The source batch/stream.
*   **Zone:** The execution context.
*   **Objects:** Stage buffers (`RawRecords`, `CleanRecords`, `TransformedRecords`).
*   **TObject:** The final load payload (`LoadPacket`).
*   **Class:** Extraction/Transformation tools (`Extractor`, `Cleaner`, `Loader`).

**Example:**
See `examples/etl_workflow.pgy`.

---

## 4. Analytical Report Pattern

**Description:**
The pattern for converting analysis results into a formal, immutable report for external consumption. It typically appears at the end of an EDA or Notebook workflow.

**Structure:**
*   Takes the final `object` views.
*   Aggregates them into a `tobject`.
*   `tobject` fields are usually read-only or finalized values.

**Example:**
See `tobject EdaReport` in `examples/eda_workflow.pgy`.
