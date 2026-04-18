import '@testing-library/jest-dom/vitest';
import { render, screen, waitFor, within, fireEvent, cleanup } from '@testing-library/svelte';
import { describe, it, expect, beforeEach, afterEach, vi } from 'vitest';
import TestHistoryCard from './TestHistoryCard.svelte';

const historyPayload = {
  tests: [
    {
      id: 11,
      createdAtUtc: '2026-04-16 12:34:56',
      pdfFile: '/artifacts/test-11.pdf',
      thumbnailFile: '/artifacts/test-11-thumb.png',
      outcome: 'pass',
      vehicle: { reg: 'AB12CDE' }
    }
  ],
  page: 1,
  perPage: 20,
  totalCount: 1,
  filters: {
    years: [2026],
    months: [{ value: 4, label: 'April' }],
    vehicles: ['AB12CDE']
  }
};

const detailsPayload = {
  test: {
    id: 11,
    createdAtUtc: '2026-04-16 12:34:56'
  },
  axleResults: [
    {
      id: 1,
      testId: 11,
      axleIndex: 1,
      testType: 'service',
      leftBrakeForce: 120,
      rightBrakeForce: 118,
      efficiency: 60,
      imbalance: 2,
      weight: 180
    },
    {
      id: 2,
      testId: 11,
      axleIndex: 2,
      testType: 'hand_brake',
      leftBrakeForce: 80,
      rightBrakeForce: 81,
      efficiency: 42,
      imbalance: 1,
      weight: 140
    }
  ]
};

function mockFetch() {
  global.fetch = vi.fn(async (input) => {
    const url = String(input);

    if (url.startsWith('/api/history?')) {
      return {
        ok: true,
        json: async () => historyPayload
      };
    }

    if (url === '/api/history/11') {
      return {
        ok: true,
        json: async () => detailsPayload
      };
    }

    return {
      ok: false,
      status: 404,
      json: async () => ({})
    };
  });
}

describe('TestHistoryCard inline details layout', () => {
  beforeEach(() => {
    vi.restoreAllMocks();
    mockFetch();
  });

  afterEach(() => {
    cleanup();
  });

  it('renders inline result details by default and does not open a result modal', async () => {
    render(TestHistoryCard);

    await screen.findByTestId('history-inline-details-11');

    expect(screen.queryByRole('dialog')).not.toBeInTheDocument();
    const sharedHeader = screen.getByTestId('history-inline-shared-header');
    expect(sharedHeader).toBeInTheDocument();
    expect(within(sharedHeader).getByText('Service Force')).toBeInTheDocument();
    expect(within(sharedHeader).getByText('Handbrake Force')).toBeInTheDocument();
    expect(screen.getByText('120 / 118')).toBeInTheDocument();
    expect(screen.queryByText('No inline details available.')).not.toBeInTheDocument();

    await fireEvent.click(screen.getByTestId('history-row-11'));
    expect(screen.queryByRole('dialog')).not.toBeInTheDocument();
  });

  it('keeps thumbnail interactive for enlargement only', async () => {
    render(TestHistoryCard);

    const thumbnailLink = await screen.findByRole('link', { name: 'Open thumbnail image' });
    expect(thumbnailLink).toHaveAttribute('href', '/artifacts/test-11-thumb.png');
    expect(thumbnailLink).toHaveAttribute('target', '_blank');
  });

  it('renders the PDF link in the metadata area under the date/time', async () => {
    render(TestHistoryCard);

    const metadata = await screen.findByTestId('history-meta-11');
    const dateTimeText = within(metadata).getByText((text) => text.includes('2026'));
    const pdfLink = within(metadata).getByTestId('history-pdf-11');

    expect(pdfLink).toHaveAttribute('href', '/artifacts/test-11.pdf');

    const relationship = dateTimeText.compareDocumentPosition(pdfLink);
    expect(relationship & Node.DOCUMENT_POSITION_FOLLOWING).toBeTruthy();
  });

  it('shows a Result label before pass/fail status text', async () => {
    render(TestHistoryCard);

    const metadata = await screen.findByTestId('history-meta-11');
    expect(within(metadata).getByText('Result:')).toBeInTheDocument();
    expect(within(metadata).getByText('Pass')).toBeInTheDocument();
  });

  it('requests per-row details during history load', async () => {
    render(TestHistoryCard);

    await waitFor(() => {
      expect(global.fetch).toHaveBeenCalledWith('/api/history/11');
    });
  });
});
